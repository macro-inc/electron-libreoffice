// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_client.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
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

void OfficeClient::HandleLOKCallback(int type,
                                     const char* payload,
                                     void* office_client) {
  static_cast<OfficeClient*>(office_client)->EmitLOKEvent(type, payload);
}

OfficeClient::OfficeClient() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE});

  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path))
    return;

  base::FilePath libreoffice_path =
      module_path.Append(FILE_PATH_LITERAL("libreofficekit"))
          .Append(FILE_PATH_LITERAL("program"));

  // TODO: set the user profile path to the proper directory
  // TODO: handle case where this fails, when == nullptr
  office_ = lok::lok_cpp_init(libreoffice_path.AsUTF8Unsafe().c_str());
  office_->registerCallback(&OfficeClient::HandleLOKCallback,
                            weak_factory_.GetWeakPtr().get());
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
  return event_bus_
      .Extend(gin::ObjectTemplateBuilder(isolate, GetTypeName(),
                                         constructor->InstanceTemplate()))
      .SetMethod("loadDocument", &OfficeClient::LoadDocument);
}

void OfficeClient::EmitLOKEvent(int type, const char* payload) {
  this->event_bus_.EmitLOKEvent(type, payload,
                                weak_factory_.GetWeakPtr().get());
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
  // base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(const Location
  // &from_here, OnceClosure task, OnceClosure reply)
  lok::Document* doc = GetDocument(path);
  if (!doc) {
    doc = office_->documentLoad(path.c_str(), "en-US");

    if (!doc) {
      LOG(ERROR) << "Unable to load '" << path << "': " << office_->getError();
      return v8::Null(isolate);
    }

    if (!document_map_.emplace(path.c_str(), doc).second) {
      LOG(ERROR)
          << "Unable to add LOK document to office client, out of memory?";
      return v8::Null(isolate);
    }

    // TODO: pass these options from the function call
    // initialize only on the first load
    LOG(ERROR) << "INITIALIZE FOR RENDERING";
    doc->initializeForRendering(R"({
      ".uno:ShowBorderShadow": {
        "type": "boolean",
        "value": false
      },
      ".uno:HideWhitespace": {
        "type": "boolean",
        "value": true
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
  }

  return DocumentClient::Create(isolate, office_, doc, path).ToV8();
}

bool OfficeClient::CloseDocument(const std::string& path) {
  return document_map_.erase(path) == 1;
}  // namespace electron::office

}  // namespace electron::office
