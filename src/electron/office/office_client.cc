// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_client.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/async_scope.h"
#include "office/document_client.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/office_singleton.h"
#include "shell/common/gin_converters/std_converter.h"
#include "unov8.hxx"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"

// Uncomment to log all document events
// #define DEBUG_EVENTS

namespace electron::office {

namespace {

static base::LazyInstance<
    base::ThreadLocalPointer<OfficeClient>>::DestructorAtExit lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

gin::WrapperInfo OfficeClient::kWrapperInfo = {gin::kEmbedderNativeGin};

OfficeClient* OfficeClient::GetCurrent() {
  OfficeClient* self = lazy_tls.Pointer()->Get();
  return self ? self : new OfficeClient;
}

bool OfficeClient::IsValid() {
  return lazy_tls.IsCreated() && lazy_tls.Pointer()->Get();
}

const ::UnoV8& OfficeClient::GetUnoV8() {
  return OfficeSingleton::GetOffice()->getUnoV8();
}

// static
void OfficeClient::HandleLibreOfficeCallback(int type,
                                             const char* payload,
                                             void* office_client) {
  static_cast<OfficeClient*>(office_client)
      ->EmitLibreOfficeEvent(type, payload);
}

void OfficeClient::HandleDocumentCallback(int type,
                                          const char* payload,
                                          void* callback_context) {
  DocumentCallbackContext* context =
      static_cast<DocumentCallbackContext*>(callback_context);
  if (context->invalid.IsSet() || !context->client.MaybeValid())
    return;

#ifdef DEBUG_EVENTS
  LOG(ERROR) << lok_callback::TypeToEventString(type) << " " << payload;
#endif

  context->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DocumentClient::ForwardLibreOfficeEvent, context->client,
                     type, payload ? std::string(payload) : std::string()));
}

v8::Local<v8::Object> OfficeClient::GetHandle(v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);
  if (eternal_.IsEmpty()) {
    v8::Local<v8::Object> local;
    if (GetWrapper(isolate).ToLocal(&local)) {
      eternal_.Set(isolate, local);
    } else {
      LOG(ERROR) << "Unable to prepare OfficeClient";
    }
  }

  return handle_scope.Escape(eternal_.Get(isolate));
}

// instance
namespace {
static void GetOfficeHandle(v8::Local<v8::Name> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (!isolate) {
    return;
  }

  v8::HandleScope handle_scope(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Context> context;
  if (!info.This()->GetCreationContext().ToLocal(&context)) {
    return;
  }
  v8::Context::Scope context_scope(context);

  OfficeClient* current = OfficeClient::GetCurrent();
  if (!current) {
    return;
  }

  auto ret = info.GetReturnValue();
  ret.Set(current->GetHandle(context));
}
}  // namespace

void OfficeClient::InstallToContext(v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  context->Global()
      ->SetAccessor(
          context, gin::StringToV8(isolate, office::OfficeClient::kGlobalEntry),
          GetOfficeHandle, nullptr, v8::MaybeLocal<v8::Value>(),
          v8::AccessControl::ALL_CAN_READ, v8::PropertyAttribute::ReadOnly)
      .Check();
}

void OfficeClient::RemoveFromContext(v8::Local<v8::Context> /*context*/) {
  delete this;
}

OfficeClient::OfficeClient() {
  lazy_tls.Pointer()->Set(this);
}

// TODO: try to save docs in a separate thread in the background if they are
// opened and destroyed
OfficeClient::~OfficeClient() {
  Destroy();
};

gin::ObjectTemplateBuilder OfficeClient::GetObjectTemplateBuilder(
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
      .SetMethod("on", &OfficeClient::On)
      .SetMethod("off", &OfficeClient::Off)
      .SetMethod("emit", &OfficeClient::Emit)
      .SetMethod("getFilterTypes", &OfficeClient::GetFilterTypes)
      .SetMethod("setDocumentPassword", &OfficeClient::SetDocumentPassword)
      .SetMethod("getVersionInfo", &OfficeClient::GetVersionInfo)
      .SetMethod("runMacro", &OfficeClient::RunMacro)
      .SetMethod("sendDialogEvent", &OfficeClient::SendDialogEvent)
      .SetMethod("loadDocument", &OfficeClient::LoadDocumentAsync)
      .SetMethod("loadDocumentFromArrayBuffer",
                 &OfficeClient::LoadDocumentFromArrayBuffer)
      .SetMethod("_beforeunload", &OfficeClient::Destroy);
}

void OfficeClient::Destroy() {
  if (destroyed_)
    return;

  destroyed_ = true;
  auto it = document_map_.cbegin();
  while (it != document_map_.cend()) {
    CloseDocument(it++->first);
  }
  lazy_tls.Pointer()->Set(nullptr);
}

const char* OfficeClient::GetTypeName() {
  return "OfficeClient";
}

lok::Office* OfficeClient::GetOffice() {
  if (!office_) {
    office_ = OfficeSingleton::GetOffice();
  }
  return office_;
}

lok::Document* OfficeClient::GetDocument(const std::string& path) {
  lok::Document* result = nullptr;
  auto item = document_map_.find(path);
  if (item != document_map_.end())
    result = item->second;
  return result;
}

// returns true if this is the first mount
bool OfficeClient::MarkMounted(lok::Document* document) {
  return documents_mounted_.insert(document).second;
}

std::string OfficeClient::GetLastError() {
  char* err = GetOffice()->getError();
  if (err == nullptr) {
    return std::string();
  }
  std::string result(err);
  GetOffice()->freeError(err);
  return result;
}

DocumentClient* OfficeClient::PrepareDocumentClient(LOKDocWithViewId doc,
                                                    const std::string& path) {
  DocumentClient* doc_client = new DocumentClient(weak_factory_.GetWeakPtr(),
                                                  doc.first, doc.second, path);

  if (document_contexts_.find(doc.first) == document_contexts_.end()) {
    DocumentCallbackContext* context = new DocumentCallbackContext{
        base::SequencedTaskRunnerHandle::Get(), doc_client->GetWeakPtr()};

    doc_client->SetView();
    doc.first->registerCallback(OfficeClient::HandleDocumentCallback, context);
    document_contexts_.emplace(doc.first, context);
  }

  return doc_client;
}

OfficeClient::LOKDocWithViewId OfficeClient::LoadDocument(
    const std::string& path) {
  GetOffice()->setOptionalFeatures(
      LibreOfficeKitOptionalFeatures::LOK_FEATURE_NO_TILED_ANNOTATIONS);

  lok::Document* doc =
      GetOffice()->documentLoad(path.c_str(), "Language=en-US,Batch=true");

  if (!doc) {
    LOG(ERROR) << "Unable to load '" << path
               << "': " << GetOffice()->getError();
    return std::make_pair(nullptr, -1);
  }

  int view_id = doc->getView();
  return std::make_pair(doc, view_id);
}

v8::Local<v8::Promise> OfficeClient::LoadDocumentAsync(
    v8::Isolate* isolate,
    const std::string& path) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Promise::Resolver> promise =
      v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  lok::Document* doc = GetDocument(path);
  if (doc) {
    DocumentClient* doc_client =
        PrepareDocumentClient(std::make_pair(doc, doc->getView()), path);
    doc_client->GetEventBus()->SetContext(isolate,
                                          isolate->GetCurrentContext());
    v8::Local<v8::Object> v8_doc_client;
    if (!doc_client->GetWrapper(isolate).ToLocal(&v8_doc_client)) {
      promise->Resolve(isolate->GetCurrentContext(), v8::Undefined(isolate))
          .Check();
    }

    promise->Resolve(isolate->GetCurrentContext(), v8_doc_client).Check();
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&OfficeClient::LoadDocument, base::Unretained(this),
                       path),
        base::BindOnce(&OfficeClient::LoadDocumentComplete,
                       weak_factory_.GetWeakPtr(), isolate,
                       new ThreadedPromiseResolver(isolate, promise), path));
  }

  return handle_scope.Escape(promise->GetPromise());
}

void OfficeClient::LoadDocumentComplete(v8::Isolate* isolate,
                                        ThreadedPromiseResolver* resolver,
                                        const std::string& path,
                                        std::pair<lok::Document*, int> doc) {
  if (!OfficeClient::IsValid()) {
    return;
  }

  AsyncScope async(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContext(isolate);
  v8::Context::Scope context_scope(context);

  if (!doc.first) {
    resolver->Resolve(isolate, v8::Undefined(isolate)).Check();
    return;
  }

  if (!document_map_.emplace(path.c_str(), doc.first).second) {
    LOG(ERROR) << "Unable to add LOK document to office client, out of memory?";
    resolver->Resolve(isolate, v8::Undefined(isolate)).Check();
    return;
  }
  DocumentClient* doc_client = PrepareDocumentClient(doc, path);
  if (!doc_client) {
    resolver->Resolve(isolate, v8::Undefined(isolate)).Check();
    return;
  }

  doc_client->GetEventBus()->SetContext(isolate, context);
  v8::Local<v8::Object> v8_doc_client;
  if (!doc_client->GetWrapper(isolate).ToLocal(&v8_doc_client)) {
    resolver->Resolve(isolate, v8::Undefined(isolate)).Check();
  }

  resolver->Resolve(isolate, v8_doc_client).Check();

  // TODO: pass these options from the function call?
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&lok::Document::initializeForRendering,
                     base::Unretained(doc.first),
                     R"({
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
    })"));
}

v8::Local<v8::Value> OfficeClient::LoadDocumentFromArrayBuffer(
    v8::Isolate* isolate,
    v8::Local<v8::ArrayBuffer> array_buffer) {
  GetOffice();
  auto backing_store = array_buffer->GetBackingStore();
  char* data = static_cast<char*>(backing_store->Data());
  std::size_t size = backing_store->ByteLength();

  if (size == 0) {
    LOG(ERROR) << "Empty array buffer provided";
    return {};
  }

  if (office_ == nullptr) {
    LOG(ERROR) << "office_ is null";
    return {};
  }

  lok::Document* doc = office_->loadFromMemory(data, size);
  if (!doc) {
    LOG(ERROR) << "Unable to load document from memory: "
               << GetOffice()->getError();
    return {};
  }
  DocumentClient* doc_client = PrepareDocumentClient({doc, doc->getView()}, "memory://");
  v8::Local<v8::Object> v8_doc_client;
  if (!doc_client->GetWrapper(isolate).ToLocal(&v8_doc_client)) {
    return {};
  }
  return v8_doc_client;
}

bool OfficeClient::CloseDocument(const std::string& path) {
  lok::Document* doc = GetDocument(path);
  if (!doc)
    return false;

  auto doc_context_it = document_contexts_.find(doc);
  if (doc_context_it != document_contexts_.end())
    doc_context_it->second->invalid.Set();
  documents_mounted_.erase(doc);

  document_map_.erase(path);
  delete doc;

  return true;
}

void OfficeClient::EmitLibreOfficeEvent(int type, const char* payload) {
  event_bus_.EmitLibreOfficeEvent(
      type, payload ? std::string(payload) : std::string());
}

void OfficeClient::On(const std::string& event_name,
                      v8::Local<v8::Function> listener_callback) {
  event_bus_.On(event_name, listener_callback);
}

void OfficeClient::Off(const std::string& event_name,
                       v8::Local<v8::Function> listener_callback) {
  event_bus_.Off(event_name, listener_callback);
}

void OfficeClient::Emit(const std::string& event_name,
                        v8::Local<v8::Value> data) {
  event_bus_.Emit(event_name, data);
}
v8::Local<v8::Value> OfficeClient::GetFilterTypes(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  char* filter_types = GetOffice()->getFilterTypes();

  v8::MaybeLocal<v8::String> maybe_filter_types_str =
      v8::String::NewFromUtf8(isolate, filter_types);

  if (maybe_filter_types_str.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  v8::MaybeLocal<v8::Value> res =
      v8::JSON::Parse(args->GetHolderCreationContext(),
                      maybe_filter_types_str.ToLocalChecked());

  if (res.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  return res.ToLocalChecked();
}

void OfficeClient::SetDocumentPassword(const std::string& url,
                                       const std::string& password) {
  GetOffice()->setDocumentPassword(url.c_str(), password.c_str());
}

v8::Local<v8::Value> OfficeClient::GetVersionInfo(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  char* version_info = GetOffice()->getVersionInfo();

  v8::MaybeLocal<v8::String> maybe_version_info_str =
      v8::String::NewFromUtf8(isolate, version_info);

  if (maybe_version_info_str.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  v8::MaybeLocal<v8::Value> res =
      v8::JSON::Parse(args->GetHolderCreationContext(),
                      maybe_version_info_str.ToLocalChecked());

  if (res.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  return res.ToLocalChecked();
}

// TODO: Investigate correct type of args here
void OfficeClient::SendDialogEvent(uint64_t window_id, gin::Arguments* args) {
  v8::Local<v8::Value> arguments;
  v8::MaybeLocal<v8::String> maybe_arguments_str;

  char* p_arguments = nullptr;

  if (args->GetNext(&arguments)) {
    maybe_arguments_str = arguments->ToString(args->GetHolderCreationContext());
    if (!maybe_arguments_str.IsEmpty()) {
      v8::String::Utf8Value p_arguments_utf8(
          args->isolate(), maybe_arguments_str.ToLocalChecked());
      p_arguments = *p_arguments_utf8;
    }
  }
  GetOffice()->sendDialogEvent(window_id, p_arguments);
}

bool OfficeClient::RunMacro(const std::string& url) {
  return GetOffice()->runMacro(url.c_str());
}

OfficeClient::_DocumentCallbackContext::_DocumentCallbackContext(
    scoped_refptr<base::SequencedTaskRunner> task_runner_,
    base::WeakPtr<DocumentClient> client_)
    : task_runner(task_runner_), client(client_) {}

OfficeClient::_DocumentCallbackContext::~_DocumentCallbackContext() = default;

}  // namespace electron::office
