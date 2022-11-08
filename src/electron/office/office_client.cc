// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_client.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/document_client.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "shell/common/gin_converters/std_converter.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/libreofficekit/LibreOfficeKit.hxx"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace electron::office {

gin::WrapperInfo OfficeClient::kWrapperInfo = {gin::kEmbedderNativeGin};

OfficeClient* OfficeClient::GetInstance() {
  return base::Singleton<OfficeClient>::get();
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
                                          void* document) {
  OfficeClient* client = GetInstance();
  EventBus* event_router =
      client->document_event_router_[static_cast<lok::Document*>(document)];
  DCHECK(event_router);

  if (client->renderer_task_runner_)
    client->renderer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EventBus::EmitLibreOfficeEvent,
                       base::Unretained(event_router), type,
                       payload ? std::string(payload) : std::string()));
}

void OfficeClient::HandleDocumentEvent(lok::Document* document,
                                       LibreOfficeKitCallbackType type,
                                       EventBus::EventCallback callback) {
  EventBus* event_router = GetInstance()->document_event_router_[document];

  event_router->Handle(type, std::move(callback));
}

gin::Handle<OfficeClient> OfficeClient::GetHandle(v8::Isolate* isolate) {
  // TODO: this should probably be per-isolate data, or at least the runner
  // should be passed to the document callback
  OfficeClient* inst = GetInstance();
  inst->event_bus_.SetContext(isolate, isolate->GetCurrentContext());
  inst->renderer_task_runner_ = base::SequencedTaskRunnerHandle::Get();

  return gin::CreateHandle(isolate, inst);
}

// instance

bool OfficeClient::IsValid() {
  return GetInstance()->office_ != nullptr;
}

OfficeClient::OfficeClient()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path))
    return;

  base::FilePath libreoffice_path =
      module_path.Append(FILE_PATH_LITERAL("libreofficekit"))
          .Append(FILE_PATH_LITERAL("program"));

  // TODO: set the user profile path to the proper directory
  office_ = lok::lok_cpp_init(libreoffice_path.AsUTF8Unsafe().c_str());

  // this is null if init fails, no further access should occur from the
  // electron_render_frame_observer
  if (office_) {
    office_->registerCallback(&OfficeClient::HandleLibreOfficeCallback, this);
  }
}

// TODO: try to save docs in a separate thread in the background if they are
// opened and destroyed
OfficeClient::~OfficeClient() = default;

gin::ObjectTemplateBuilder OfficeClient::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> constructor =
      data->GetFunctionTemplate(&this->kWrapperInfo);
  if (constructor.IsEmpty()) {
    constructor = v8::FunctionTemplate::New(isolate);
    constructor->SetClassName(gin::StringToV8(isolate, GetTypeName()));
    constructor->ReadOnlyPrototype();
    data->SetFunctionTemplate(&this->kWrapperInfo, constructor);
  }
  return gin::ObjectTemplateBuilder(isolate, GetTypeName(),
                                    constructor->InstanceTemplate())
      .SetMethod("on", &OfficeClient::On)
      .SetMethod("off", &OfficeClient::Off)
      .SetMethod("emit", &OfficeClient::Emit)
      .SetMethod("loadDocument", &OfficeClient::LoadDocument);
}

const char* OfficeClient::GetTypeName() {
  return "OfficeClient";
}

lok::Office* OfficeClient::GetOffice() {
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
  char* err = office_->getError();
  if (err == nullptr) {
    return std::string();
  }
  std::string result(err);
  office_->freeError(err);
  return result;
}

v8::Local<v8::Value> OfficeClient::LoadDocument(v8::Isolate* isolate,
                                                const std::string& path) {
  lok::Document* doc = GetDocument(path);

  if (!doc) {
    doc = office_->documentLoad(path.c_str(), "en-US");

    if (!doc) {
      LOG(ERROR) << "Unable to load '" << path << "': " << office_->getError();
      return v8::Undefined(isolate);
    }

    if (!document_map_.emplace(path.c_str(), doc).second) {
      LOG(ERROR)
          << "Unable to add LOK document to office client, out of memory?";
      return v8::Undefined(isolate);
    }

    // TODO: pass these options from the function call?
    doc->initializeForRendering(R"({
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
        "value": "Your Friendly Neighborhood Author"
      }
    })");

    document_event_router_[doc] = new EventBus();

    doc->registerCallback(OfficeClient::HandleDocumentCallback, doc);
  }

  DocumentClient* doc_client =
      new DocumentClient(doc, path, document_event_router_[doc]);
  return gin::CreateHandle(isolate, doc_client).ToV8();
}

bool OfficeClient::CloseDocument(const std::string& path) {
  lok::Document* doc = document_map_[path];
  document_event_router_.erase(doc);
  return document_map_.erase(path) == 1;
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

}  // namespace electron::office
