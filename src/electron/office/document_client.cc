// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/document_client.h"
#include <sys/types.h>

#include <memory>
#include <string_view>
#include <vector>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/memory.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/document_holder.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "office/office_instance.h"
#include "office/promise.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "shell/common/gin_converters/std_converter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "unov8.hxx"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8_stringify.h"

namespace electron::office {
gin::WrapperInfo DocumentClient::kWrapperInfo = {gin::kEmbedderNativeGin};

namespace {

#if BUILDFLAG(IS_WIN)
inline HANDLE get_heap_handle() {
  return reinterpret_cast<HANDLE>(_get_heap_handle());
}
#endif

inline void lok_safe_free(void* ptr) {
#if BUILDFLAG(IS_WIN)
  if (!ptr) {
    return;
  }
  HeapFree(get_heap_handle(), 0, ptr);
#else
  base::UncheckedFree(ptr);
#endif
}

struct LokSafeDeleter {
  inline void operator()(void* ptr) const { lok_safe_free(ptr); }
};

typedef std::unique_ptr<char[], LokSafeDeleter> LokStrPtr;

std::unique_ptr<char[]> jsonStringify(const v8::Local<v8::Context>& context,
                                      const v8::Local<v8::Value>& val) {
  if (val->IsUndefined())
    return {};
  v8::Local<v8::String> str_object;
  if (!v8::JSON::Stringify(context, val).ToLocal(&str_object))
    return {};
  return v8_stringify(context, str_object);
}

}  // namespace

DocumentClient::DocumentClient() = default;
DocumentClient::DocumentClient(DocumentHolderWithView holder)
    : document_holder_(std::move(holder)) {
  // assumes the document loaded succesfully from OfficeClient
  DCHECK(document_holder_);
  DCHECK(OfficeInstance::IsValid());

  static constexpr int internal_monitors[] = {
      LOK_CALLBACK_DOCUMENT_SIZE_CHANGED,
      LOK_CALLBACK_INVALIDATE_TILES,
      LOK_CALLBACK_STATE_CHANGED,
  };
  for (auto event_type : internal_monitors) {
    document_holder_.AddDocumentObserver(event_type, this);
    event_types_registered_.emplace(event_type);
  }
  OfficeInstance::Get()->AddDestroyedObserver(this);
}

DocumentClient::~DocumentClient() {
  if (document_holder_) {
    document_holder_.RemoveDocumentObservers();
  }
  OfficeInstance::Get()->RemoveDestroyedObserver(this);
}

// gin::Wrappable
gin::ObjectTemplateBuilder DocumentClient::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> constructor =
      data->GetFunctionTemplate(&kWrapperInfo);
  if (constructor.IsEmpty()) {
    constructor = v8::FunctionTemplate::New(isolate);
    constructor->SetClassName(gin::StringToV8(isolate, GetTypeName()));
    constructor->ReadOnlyPrototype();
    data->SetFunctionTemplate(&kWrapperInfo, constructor);
  }
  return gin::ObjectTemplateBuilder(isolate, GetTypeName(),
                                    constructor->InstanceTemplate())
      .SetMethod("on", &DocumentClient::On)
      .SetMethod("off", &DocumentClient::Off)
      .SetMethod("emit", &DocumentClient::Emit)
      .SetMethod("postUnoCommand", &DocumentClient::PostUnoCommand)
      .SetMethod("setAuthor", &DocumentClient::SetAuthor)
      .SetMethod("gotoOutline", &DocumentClient::GotoOutline)
      .SetMethod("saveToMemory", &DocumentClient::SaveToMemory)
      .SetMethod("saveAs", &DocumentClient::SaveAs)
      .SetMethod("setTextSelection", &DocumentClient::SetTextSelection)
      .SetMethod("getClipboard", &DocumentClient::GetClipboard)
      .SetMethod("setClipboard", &DocumentClient::SetClipboard)
      .SetMethod("paste", &DocumentClient::Paste)
      .SetMethod("setGraphicSelection", &DocumentClient::SetGraphicSelection)
      .SetMethod("resetSelection", &DocumentClient::ResetSelection)
      .SetMethod("getCommandValues", &DocumentClient::GetCommandValues)
      .SetMethod("as", &DocumentClient::As)
      .SetMethod("newView", &DocumentClient::NewView)
      .SetProperty("isReady", &DocumentClient::IsReady)
      .SetMethod("initializeForRendering",
                 &DocumentClient::InitializeForRendering);
}

const char* DocumentClient::GetTypeName() {
  return "DocumentClient";
}

bool DocumentClient::IsReady() const {
  return is_ready_;
}

std::vector<gfx::Rect> DocumentClient::PageRects() const {
  return page_rects_;
}

gfx::Size DocumentClient::DocumentSizeTwips() {
  return gfx::Size(document_width_in_twips_, document_height_in_twips_);
}

bool DocumentClient::Mount(v8::Isolate* isolate) {
  bool first_mount = mount_counter_.Increment() == 0;

  if (!first_mount)
    return false;

  v8::Local<v8::Value> wrapper;
  if (GetWrapper(isolate).ToLocal(&wrapper)) {
    mounted_.Reset(isolate, wrapper);
  } else {
    LOG(ERROR) << "unable to mount document client";
  }

  RefreshSize();

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DocumentClient::EmitReady, GetWeakPtr(), isolate,
          v8::Global<v8::Context>(isolate, isolate->GetCurrentContext())));

  return true;
}

bool DocumentClient::Unmount() {
  bool not_last_mount = mount_counter_.Decrement();
  if (not_last_mount)
    return false;

  mounted_.Reset();

  return true;
}

int DocumentClient::GetNumberOfPages() const {
  return document_holder_->getParts();
}
//}

// Editing State {
bool DocumentClient::CanUndo() {
	return can_undo_;
}

bool DocumentClient::CanRedo() {
	return can_redo_;
}
// }

void DocumentClient::MarkRendererWillRemount(
    base::Token restore_key,
    RendererTransferable&& renderer_transferable) {
  tile_buffers_to_restore_[std::move(restore_key)] =
      std::move(renderer_transferable);
}

RendererTransferable DocumentClient::GetRestoredRenderer(
    const base::Token& restore_key) {
  auto it = tile_buffers_to_restore_.find(restore_key);
  if (it == tile_buffers_to_restore_.end()) {
    return {};
  }

  RendererTransferable res = std::move(it->second);
  tile_buffers_to_restore_.erase(it);
  return res;
}

DocumentHolderWithView DocumentClient::GetDocument() {
  return document_holder_;
}

base::WeakPtr<DocumentClient> DocumentClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DocumentClient::HandleStateChange(const std::string& payload) {
	static constexpr std::string_view uno_undo = ".uno:Undo=";
	static constexpr std::string_view uno_redo = ".uno:Redo=";
	std::string_view sv = payload;
  if (sv.substr(0, uno_undo.length()) == uno_undo) {
		can_undo_ = sv.substr(uno_undo.length()) == "enabled";
  }
  if (sv.substr(0, uno_redo.length()) == uno_redo) {
		can_redo_ = sv.substr(uno_redo.length()) == "enabled";
  }

  if (!is_ready_) {
    state_change_buffer_.emplace_back(payload);
  }
}

void DocumentClient::HandleDocSizeChanged() {
  RefreshSize();
}

void DocumentClient::HandleInvalidate() {
  is_ready_ = true;
}

void DocumentClient::RefreshSize() {
  document_holder_->getDocumentSize(&document_width_in_twips_,
                                    &document_height_in_twips_);

  LokStrPtr page_rect(document_holder_->getPartPageRectangles());
  std::string_view page_rect_sv(page_rect.get());
  std::string_view::const_iterator start = page_rect_sv.begin();
  int new_size = GetNumberOfPages();
  page_rects_ =
      lok_callback::ParseMultipleRects(start, page_rect_sv.end(), new_size);
}

void DocumentClient::On(v8::Isolate* isolate,
                        const std::u16string& event_name,
                        v8::Local<v8::Function> listener_callback) {
  int type = lok_callback::EventStringToType(event_name);
  if (type < 0) {
    LOG(ERROR) << "on, unknown event: " << event_name;
  }
  event_listeners_[type].emplace_back(isolate, listener_callback);
  if (event_types_registered_.emplace(type).second) {
    document_holder_.AddDocumentObserver(type, this);
  }

  // store the isolate for emitting callbacks later in ForwardEmit
  if (!isolate_) {
    isolate_ = isolate;
  } else {
    DCHECK(isolate_ == isolate);
  }
}

void DocumentClient::Off(const std::u16string& event_name,
                         v8::Local<v8::Function> listener_callback) {
  int type = lok_callback::EventStringToType(event_name);
  if (type < 0) {
    LOG(ERROR) << "off, unknown event: " << event_name;
  }
  auto itr = event_listeners_.find(type);
  if (itr == event_listeners_.end())
    return;

  auto& vec = itr->second;
  vec.erase(std::remove(vec.begin(), vec.end(), listener_callback), vec.end());
  // This would be used to remove observers, but in reality they're likely to
  // be-reregistered with a different function, and this would remove the
  // internal monitors.
  /* if (vec.size() == 0) {
// 	event_types_registered_.erase(type);
// 	document_holder_.RemoveDocumentObserver(type, this);
}*/
}

void DocumentClient::Emit(v8::Isolate* isolate,
                          const std::u16string& event_name,
                          v8::Local<v8::Value> data) {
  int type = lok_callback::EventStringToType(event_name);
  if (type < 0) {
    LOG(ERROR) << "emit, unknown event: " << event_name;
  }
  auto itr = event_listeners_.find(type);
  if (itr == event_listeners_.end())
    return;
  for (auto& callback : itr->second) {
    V8FunctionInvoker<void(v8::Local<v8::Value>)>::Go(isolate, callback, data);
  }
}

v8::Local<v8::Value> DocumentClient::GotoOutline(int idx,
                                                 gin::Arguments* args) {
  LokStrPtr result(document_holder_->gotoOutline(idx));
  v8::Isolate* isolate = args->isolate();

  if (!result) {
    return v8::Undefined(isolate);
  }

  v8::Local<v8::String> json_str;

  if (!v8::String::NewFromUtf8(isolate, result.get()).ToLocal(&json_str)) {
    return v8::Undefined(isolate);
  }

  return v8::JSON::Parse(args->GetHolderCreationContext(), json_str)
      .FromMaybe(v8::Local<v8::Value>());
}

namespace {
#if BUILDFLAG(IS_WIN)
extern "C" void* (*const malloc_unchecked)(size_t);
#endif

void* UncheckedAlloc(size_t size) {
#if BUILDFLAG(IS_WIN)
  return malloc_unchecked(size);
#else
  void* ptr;
  std::ignore = base::UncheckedMalloc(size, &ptr);
  return ptr;
#endif
}

}  // namespace

v8::Local<v8::Promise> DocumentClient::SaveToMemory(v8::Isolate* isolate,
                                                    gin::Arguments* args) {
  Promise<v8::Value> promise(isolate);
  auto handle = promise.GetHandle();
  v8::Local<v8::Value> arguments;
  std::unique_ptr<char[]> format;
  if (args->GetNext(&arguments)) {
    format = v8_stringify(isolate->GetCurrentContext(), arguments);
  }

  document_holder_.PostBlocking(base::BindOnce(
      [](Promise<v8::Value> promise, std::unique_ptr<char[]> format,
         base::WeakPtr<OfficeClient> office, DocumentHolderWithView holder) {
        if (!office.MaybeValid())
          return;
        char* pOutput = nullptr;
        size_t size = holder->saveToMemory(&pOutput, UncheckedAlloc,
                                           format ? format.get() : nullptr);

        if (size <= 0) {
          Promise<v8::Value>::ResolvePromise(std::move(promise));
          return;
        }

        promise.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](Promise<v8::Value> promise, char* data, size_t size,
                   base::WeakPtr<OfficeClient> office) {
                  if (!office.MaybeValid())
                    return;
                  v8::Isolate* isolate = promise.isolate();
                  v8::HandleScope handle_scope(isolate);
                  v8::MicrotasksScope microtasks_scope(
                      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
                  v8::Context::Scope context_scope(promise.GetContext());

                  // since buffer.data() is from a dangling malloc, add a
                  // free(...) deleter
                  auto backing_store = v8::ArrayBuffer::NewBackingStore(
                      data, size,
                      [](void* data, size_t, void*) { lok_safe_free(data); },
                      nullptr);
                  v8::Local<v8::ArrayBuffer> array_buffer =
                      v8::ArrayBuffer::New(isolate, std::move(backing_store));
                  promise.Resolve(std::move(array_buffer));
                },
                std::move(promise), pOutput, size, std::move(office)));
      },
      std::move(promise), std::move(format), OfficeClient::GetWeakPtr()));

  return handle;
}

void DocumentClient::SetAuthor(const std::string& author,
                               gin::Arguments* args) {
  document_holder_->setAuthor(author.c_str());
}

void DocumentClient::PostUnoCommand(const std::string& command,
                                    gin::Arguments* args) {
  v8::Local<v8::Value> arguments;
  std::unique_ptr<char[]> json_buffer;

  bool notifyWhenFinished = false;
  if (args->GetNext(&arguments) && !arguments->IsUndefined()) {
    json_buffer = jsonStringify(args->GetHolderCreationContext(), arguments);
    if (!json_buffer)
      return;
  }

  args->GetNext(&notifyWhenFinished);

  PostUnoCommandInternal(command, std::move(json_buffer), notifyWhenFinished);
}

void DocumentClient::PostUnoCommandInternal(const std::string& command,
                                            std::unique_ptr<char[]> json_buffer,
                                            bool notifyWhenFinished) {
  document_holder_->postUnoCommand(command.c_str(), json_buffer.get(),
                                   notifyWhenFinished);
}

void DocumentClient::SetTextSelection(int n_type, int n_x, int n_y) {
  document_holder_->setTextSelection(n_type, n_x, n_y);
}

namespace {

v8::Local<v8::Value> lok_clipboard_to_buffer(v8::Isolate* isolate,
                                             const char* mime_type,
                                             const char* stream,
                                             size_t size) {
  // allocate a new ArrayBuffer and copy the stream to the backing
  // store
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, size);
  std::memcpy(buffer->GetBackingStore()->Data(), stream, size);

  v8::Local<v8::Name> names[2] = {gin::StringToV8(isolate, "mimeType"),
                                  gin::StringToV8(isolate, "buffer")};
  v8::Local<v8::Value> values[2] = {gin::StringToV8(isolate, mime_type),
                                    gin::ConvertToV8(isolate, buffer)};

  return v8::Object::New(isolate, v8::Null(isolate), names, values, 2);
}

v8::Local<v8::Value> lok_clipboard_to_string(v8::Isolate* isolate,
                                             const char* mime_type,
                                             const char* stream) {
  v8::Local<v8::Name> names[2] = {gin::StringToV8(isolate, "mimeType"),
                                  gin::StringToV8(isolate, "text")};
  v8::Local<v8::Value> values[2] = {gin::StringToV8(isolate, mime_type),
                                    gin::StringToV8(isolate, stream)};

  return v8::Object::New(isolate, v8::Null(isolate), names, values, 2);
}

}  // namespace

v8::Local<v8::Value> DocumentClient::GetClipboard(gin::Arguments* args) {
  std::vector<std::string> mime_types;
  std::vector<const char*> mime_c_str;
  static constexpr std::string_view text_plain = "text/plain";

  if (args->GetNext(&mime_types)) {
    for (const std::string& mime_type : mime_types) {
      // LOK explicitly converts all UTF-16 strings to UTF-8, however it still
      // requests an encoding
      if (mime_type == text_plain) {
        mime_c_str.push_back("text/plain;charset=utf-8");
        continue;
      }
      // c_str() gaurantees that the string is null-terminated, data()
      // does not, don't use data() or bad things will happen
      mime_c_str.push_back(mime_type.c_str());
    }

    // add the nullptr terminator to the list of null-terminated strings
    mime_c_str.push_back(nullptr);
  }

  size_t out_count;

  // these are arrays of out_count size, variable size arrays in C are
  // simply pointers to the first element
  char** out_mime_types = nullptr;
  size_t* out_sizes = nullptr;
  char** out_streams = nullptr;

  bool success = document_holder_->getClipboard(
      mime_types.size() ? mime_c_str.data() : nullptr, &out_count,
      &out_mime_types, &out_sizes, &out_streams);

  // we'll be refrencing this and the context frequently inside of the
  // loop
  v8::Isolate* isolate = args->isolate();

  // return an empty array if we failed
  if (!success)
    return v8::Array::New(isolate, 0);

  // an array of n=out_count array buffers
  v8::Local<v8::Array> result = v8::Array::New(isolate, out_count);
  // we pull out the context once outside of the array
  v8::Local<v8::Context> context = args->GetHolderCreationContext();

  for (size_t i = 0; i < out_count; ++i) {
    size_t buffer_size = out_sizes[i];
    if (buffer_size <= 0) {
      std::ignore = result->Set(context, i, v8::Undefined(isolate_));
      continue;
    }
    static constexpr std::string_view text_prefix = "text/";
    std::string_view sv_mime_type(out_mime_types[i]);
    if (sv_mime_type.substr(0, text_prefix.length()) == text_prefix) {
      if (sv_mime_type.substr(0, text_plain.length()) == text_plain) {
        std::ignore =
            result->Set(context, i,
                        lok_clipboard_to_string(isolate_, text_plain.data(),
                                                out_streams[i]));
      } else {
        std::ignore =
            result->Set(context, i,
                        lok_clipboard_to_string(isolate_, out_mime_types[i],
                                                out_streams[i]));
      }
    } else {
      std::ignore =
          result->Set(context, i,
                      lok_clipboard_to_buffer(isolate_, out_mime_types[i],
                                              out_streams[i], out_sizes[i]));
    }

    // free the clipboard item, can't use std::unique_ptr without a
    // wrapper class since it needs to be size aware
    lok_safe_free(out_streams[i]);
    lok_safe_free(out_mime_types[i]);
  }
  // free the clipboard item containers
  lok_safe_free(out_sizes);
  lok_safe_free(out_streams);
  lok_safe_free(out_mime_types);

  return result;
}

bool DocumentClient::SetClipboard(
    std::vector<v8::Local<v8::Object>> clipboard_data,
    gin::Arguments* args) {
  // entries in clipboard_data
  const size_t entries = clipboard_data.size();

  // No entries in clipboard_data
  if (entries == 0) {
    return false;
  }

  std::vector<const char*> mime_c_str;
  size_t in_sizes[entries];
  const char* streams[entries];

  v8::Isolate* isolate = args->isolate();
  for (size_t i = 0; i < entries; ++i) {
    gin::Dictionary dictionary(isolate, clipboard_data[i]);

    std::string mime_type;
    dictionary.Get<std::string>("mimeType", &mime_type);

    v8::Local<v8::ArrayBuffer> buffer;
    dictionary.Get<v8::Local<v8::ArrayBuffer>>("buffer", &buffer);

    in_sizes[i] = buffer->ByteLength();
    mime_c_str.push_back(mime_type.c_str());
    streams[i] = static_cast<char*>(buffer->GetBackingStore()->Data());
  }

  // add the nullptr terminator to the list of null-terminated strings
  mime_c_str.push_back(nullptr);

  return document_holder_->setClipboard(entries, mime_c_str.data(), in_sizes,
                                        streams);
}

bool DocumentClient::Paste(const std::string& mime_type,
                           const std::string& data,
                           gin::Arguments* args) {
  return document_holder_->paste(mime_type.c_str(), data.c_str(), data.size());
}

void DocumentClient::SetGraphicSelection(int n_type, int n_x, int n_y) {
  document_holder_->setGraphicSelection(n_type, n_x, n_y);
}

void DocumentClient::ResetSelection() {
  document_holder_->resetSelection();
}

v8::Local<v8::Promise> DocumentClient::GetCommandValues(
    const std::string& command,
    gin::Arguments* args) {
  Promise<v8::Value> promise(args->isolate());
  auto handle = promise.GetHandle();

  document_holder_.Post(base::BindOnce(
      [](Promise<v8::Value> promise, std::string command,
         base::WeakPtr<OfficeClient> office,
         DocumentHolderWithView doc_holder) {
        LokStrPtr result(doc_holder->getCommandValues(command.c_str()));
        if (!result) {
          return promise.Resolve();
        }
        promise.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](Promise<v8::Value> promise, LokStrPtr result,
                   base::WeakPtr<OfficeClient> office) {
                  if (!office.MaybeValid())
                    return;
                  v8::Isolate* isolate = promise.isolate();
                  v8::HandleScope handle_scope(isolate);
                  v8::MicrotasksScope microtasks_scope(
                      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
                  v8::Context::Scope context_scope(promise.GetContext());

                  v8::Local<v8::String> res_json_str;
                  if (!v8::String::NewFromUtf8(promise.isolate(), result.get())
                           .ToLocal(&res_json_str)) {
                    return promise.Resolve();
                  }
                  promise.Resolve(
                      v8::JSON::Parse(promise.GetContext(), res_json_str)
                          .FromMaybe(v8::Local<v8::Value>()));
                },
                std::move(promise), std::move(result), std::move(office)));
      },
      std::move(promise), command, OfficeClient::GetWeakPtr()));

  return handle;
}

v8::Local<v8::Value> DocumentClient::As(const std::string& type,
                                        v8::Isolate* isolate) {
  void* component = document_holder_->getXComponent();
  return convert::As(isolate, component, type);
}

v8::Local<v8::Value> DocumentClient::NewView(v8::Isolate* isolate) {
  auto* new_client = new DocumentClient(document_holder_.NewView());
  v8::Local<v8::Object> result;

  if (!new_client->GetWrapper(isolate).ToLocal(&result))
    return v8::Undefined(isolate);

  return result;
}

void DocumentClient::EmitReady(v8::Isolate* isolate,
                               v8::Global<v8::Context> context) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Context> context_ = context.Get(isolate);
  v8::Context::Scope context_scope(context_);

  // emit the state change buffer when ready
  v8::Local<v8::Array> ready_value =
      v8::Array::New(isolate, state_change_buffer_.size());

  if (!state_change_buffer_.empty()) {
    for (size_t i = 0; i < state_change_buffer_.size(); i++) {
      ready_value
          ->Set(context_, i,
                lok_callback::PayloadToLocalValue(
                    isolate, LOK_CALLBACK_STATE_CHANGED,
                    state_change_buffer_[i].c_str()))
          .Check();
    }

    state_change_buffer_.clear();
  }

  Emit(isolate, u"ready", ready_value);
}

void DocumentClient::ForwardEmit(int type, const std::string& payload) {
  auto itr = event_listeners_.find(type);
  if (itr == event_listeners_.end())
    return;
  DCHECK(isolate_);
  for (auto& callback : itr->second) {
    V8FunctionInvoker<void(EventPayload)>::Go(isolate_, callback,
                                              {type, payload});
  }
}

v8::Local<v8::Promise> DocumentClient::SaveAs(v8::Isolate* isolate,
                                              gin::Arguments* args) {
  v8::Local<v8::Value> arguments;
  std::unique_ptr<char[]> path;
  std::unique_ptr<char[]> format;
  std::unique_ptr<char[]> options;

  if (!args->GetNext(&arguments)) {
    args->ThrowTypeError("missing path");
    return {};
  }
  path = v8_stringify(isolate->GetCurrentContext(), arguments);
  if (args->GetNext(&arguments)) {
    format = v8_stringify(isolate->GetCurrentContext(), arguments);
  }
  if (args->GetNext(&arguments)) {
    options = v8_stringify(isolate->GetCurrentContext(), arguments);
  }
  Promise<bool> promise(isolate);
  auto holder = promise.GetHandle();

  document_holder_.PostBlocking(base::BindOnce(
      [](std::unique_ptr<char[]> path, std::unique_ptr<char[]> format,
         std::unique_ptr<char[]> options, Promise<bool> promise,
         base::WeakPtr<OfficeClient> office, DocumentHolderWithView doc) {
        bool res = doc->saveAs(path.get(), format ? format.get() : nullptr,
                               options ? options.get() : nullptr);
        Promise<bool>::ResolvePromise(std::move(promise), res);
      },
      std::move(path), std::move(format), std::move(options),
      std::move(promise), OfficeClient::GetWeakPtr()));

  return holder;
}

v8::Local<v8::Promise> DocumentClient::InitializeForRendering(
    v8::Isolate* isolate) {
  document_holder_.PostBlocking(base::BindOnce(
      [](base::WeakPtr<OfficeClient> office, DocumentHolderWithView holder) {
        static constexpr const char* options = R"({
					".uno:ShowBorderShadow": {
						"type": "boolean",
						"value": false
					},
					".uno:HideWhitespace": {
						"type": "boolean",
						"value": false
					},
					".uno:SpellOnline": {
						"type": "boolean",
						"value": false
					},
					".uno:Author": {
						"type": "string",
						"value": "Macro User"
					}
				})";
        holder->initializeForRendering(options);
      },
      OfficeClient::GetWeakPtr()));
  return {};
}

void DocumentClient::DocumentCallback(int type, std::string payload) {
  switch (static_cast<LibreOfficeKitCallbackType>(type)) {
      // internal monitors
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
      HandleDocSizeChanged();
      ForwardEmit(type, payload);
      break;
    case LOK_CALLBACK_INVALIDATE_TILES:
      HandleInvalidate();
      ForwardEmit(type, payload);
      break;
    case LOK_CALLBACK_STATE_CHANGED:
      HandleStateChange(payload);
      ForwardEmit(type, payload);
      break;
    default:
      ForwardEmit(type, payload);
      break;
  }
}

void DocumentClient::OnDestroyed() {
  delete this;
}

}  // namespace electron::office
