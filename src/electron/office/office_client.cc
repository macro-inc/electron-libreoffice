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
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/token.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/async_scope.h"
#include "office/document_client.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/office_instance.h"
#include "office/threaded_promise_resolver.h"
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
v8::Global<v8::Value>* Instance() {
  static base::NoDestructor<v8::Global<v8::Value>> inst;
  return inst.get();
}
}  // namespace

gin::WrapperInfo OfficeClient::kWrapperInfo = {gin::kEmbedderNativeGin};

void OfficeClient::OnLoaded(lok::Office* client) {
  office_ = client;
  ::UnoV8Instance::Set(client->getUnoV8());
  loaded_.Signal();
  LOG(ERROR) << "LOK LOADED!";
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

// instance
namespace {
static void GetOfficeHandle(v8::Local<v8::Name> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (!isolate) {
    NOTREACHED();
    return;
  }
  v8::Local<v8::Context> context;
  if (!info.This()->GetCreationContext().ToLocal(&context)) {
    NOTREACHED();
    return;
  }
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  DCHECK(!Instance()->IsEmpty());
  info.GetReturnValue().Set(*Instance());
}
}  // namespace

void OfficeClient::InstallToContext(v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (Instance()->IsEmpty()) {
    OfficeClient* client = new OfficeClient;
    v8::Local<v8::Object> wrapper;
    if (!client->GetWrapper(isolate).ToLocal(&wrapper) || wrapper.IsEmpty()) {
      NOTREACHED();
    }
    Instance()->Reset(isolate, wrapper);
  }

  context->Global()
      ->SetAccessor(
          context, gin::StringToV8(isolate, office::OfficeClient::kGlobalEntry),
          GetOfficeHandle, nullptr, v8::MaybeLocal<v8::Value>(),
          v8::AccessControl::ALL_CAN_READ, v8::PropertyAttribute::ReadOnly)
      .Check();
}

void OfficeClient::RemoveFromContext(v8::Local<v8::Context> /*context*/) {
  Instance()->Reset();
}

OfficeClient::OfficeClient()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  OfficeInstance::Get()->AddLoadObserver(this);
}

OfficeClient::~OfficeClient() {
  OfficeInstance::Get()->RemoveLoadObserver(this);
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
      .SetMethod("setDocumentPassword", &OfficeClient::SetDocumentPasswordAsync)
      .SetMethod("loadDocument", &OfficeClient::LoadDocumentAsync)
      .SetMethod("loadDocumentFromArrayBuffer",
                 &OfficeClient::LoadDocumentFromArrayBuffer);
}

const char* OfficeClient::GetTypeName() {
  return "OfficeClient";
}

lok::Office* OfficeClient::GetOffice() {
  return office_;
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

DocumentClient* OfficeClient::PrepareDocumentClient(lok::Document* document) {
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
  auto load_document_ =
      base::BindOnce(&OfficeClient::LoadDocument, base::Unretained(this), path);
  auto load_document_complete_ = base::BindPostTask(
      task_runner_,
      base::BindOnce(&OfficeClient::LoadDocumentComplete,
                     weak_factory_.GetWeakPtr(), isolate,
                     ThreadedPromiseResolver(isolate, promise), path));
  if (loaded_.is_signaled()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        std::move(load_document_), std::move(load_document_complete_));
  } else {
    loaded_.Post(
        FROM_HERE,
        base::BindOnce(
            [](decltype(load_document_) load,
               decltype(load_document_complete_) complete) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE,
                  {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
                  std::move(load), std::move(complete));
            },
            std::move(load_document_), std::move(load_document_complete_)));
  }

  return handle_scope.Escape(promise->GetPromise());
}

void OfficeClient::LoadDocumentComplete(v8::Isolate* isolate,
                                        ThreadedPromiseResolver resolver,
                                        const std::string& path,
                                        std::pair<lok::Document*, int> doc) {
  AsyncScope async(isolate);
  v8::Local<v8::Context> context = resolver.GetCreationContext();
  v8::Context::Scope context_scope(context);

  if (!doc.first) {
    resolver.Resolve({});
    return;
  }

  if (!document_map_.emplace(path.c_str(), doc.first).second) {
    LOG(ERROR) << "Unable to add LOK document to office client, out of memory?";
    resolver.Resolve({});
    return;
  }
  DocumentClient* doc_client = PrepareDocumentClient(doc, path);
  if (!doc_client) {
    resolver.Resolve({});
    return;
  }

  doc_client->GetEventBus()->SetContext(isolate, context);
  v8::Local<v8::Object> v8_doc_client;
  if (!doc_client->GetWrapper(isolate).ToLocal(&v8_doc_client)) {
    resolver.Resolve({});
  }

  resolver.Resolve(v8_doc_client);

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

v8::Local<v8::Promise> OfficeClient::LoadDocumentFromArrayBuffer(
    v8::Isolate* isolate,
    v8::Local<v8::ArrayBuffer> array_buffer) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Promise::Resolver> promise =
      v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
  if (array_buffer->ByteLength() == 0) {
    LOG(ERROR) << "Empty array buffer provided";
    base::IgnoreResult(promise->Resolve(isolate->GetCurrentContext(), {}));
    return handle_scope.Escape(promise->GetPromise());
  }
  const std::string path = "memory://" + base::Token::CreateRandom().ToString();
  auto load_document_complete_ = base::BindPostTask(
      task_runner_,
      base::BindOnce(&OfficeClient::LoadDocumentComplete,
                     weak_factory_.GetWeakPtr(), isolate,
                     ThreadedPromiseResolver(isolate, promise), path));

  auto load_document_ = base::BindOnce(
      [](OfficeClient& office_client, v8::Isolate* isolate,
         v8::Global<v8::ArrayBuffer> global,
         decltype(load_document_complete_) complete) {
        // lambda
        v8::Local<v8::ArrayBuffer> array_buffer = global.Get(isolate);
        array_buffer->GetBackingStore()->Data();
        lok::Document* office = office_client.GetOffice()->loadFromMemory(
            static_cast<char*>(array_buffer->GetBackingStore()->Data()),
            array_buffer->ByteLength());
        load_document_complete_.Run(pair<Document*, int> args);
      },
      // dependencies
      this, isolate, v8::Global<v8::ArrayBuffer>(isolate, array_buffer),
      std::move(load_document_complete_));

  if (loaded_.is_signaled()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        std::move(load_document_), std::move(load_document_complete_));
  } else {
    loaded_.Post(
        FROM_HERE,
        base::BindOnce(
            [](decltype(load_document_) load,
               decltype(load_document_complete_) complete) {
              // lambda
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE,
                  {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
                  std::move(load), std::move(complete));
            },
            // dependencies
            std::move(load_document_), std::move(load_document_complete_)));
  }

  if (loaded_.is_signaled()) {
  } else {
  }

  if (!doc) {
    LOG(ERROR) << "Unable to load document from memory: " << GetLastError();
    base::IgnoreResult(promise->Resolve(isolate->GetCurrentContext(), {}));
    return handle_scope.Escape(promise->GetPromise());
  }

  DocumentClient* doc_client = PrepareDocumentClient(
      {doc, doc->createView()},
      "memory://" + base::Token::CreateRandom().ToString());
  doc_client->GetEventBus()->SetContext(isolate, isolate->GetCurrentContext());
  v8::Local<v8::Object> v8_doc_client;
  if (!doc_client->GetWrapper(isolate).ToLocal(&v8_doc_client)) {
    return {};
  }

  return handle_scope.Escape(promise->GetPromise());
}

v8::Local<v8::Promise> OfficeClient::SetDocumentPasswordAsync(
    v8::Isolate* isolate,
    const std::string& url,
    const std::string& password) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Promise::Resolver> promise =
      v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  if (loaded_.is_signaled()) {
    GetOffice()->setDocumentPassword(url.c_str(), password.c_str());
    base::IgnoreResult(promise->Resolve(isolate->GetCurrentContext(), {}));
  } else {
    auto async = base::BindOnce(
        [](OfficeClient& client, ThreadedPromiseResolver promise,
           std::string& url, std::string& password) {
          client.GetOffice()->setDocumentPassword(url.c_str(),
                                                  password.c_str());
          promise.Resolve({});
        },
        this, ThreadedPromiseResolver(isolate, std::move(promise)),
        std::move(url), std::move(password));

    loaded_.Post(FROM_HERE, base::BindPostTask(task_runner_, std::move(async)));
  }

  return handle_scope.Escape(promise->GetPromise());
}

OfficeClient::_DocumentCallbackContext::_DocumentCallbackContext(
    scoped_refptr<base::SequencedTaskRunner> task_runner_,
    base::WeakPtr<DocumentClient> client_)
    : task_runner(task_runner_), client(client_) {}

OfficeClient::_DocumentCallbackContext::~_DocumentCallbackContext() = default;
}  // namespace electron::office
