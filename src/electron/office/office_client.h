// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_CLIENT_H_
#define OFFICE_OFFICE_CLIENT_H_

#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/one_shot_event.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_local.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office/document_holder.h"
#include "office/event_bus.h"
#include "office/threaded_promise_resolver.h"
#include "office_load_observer.h"
#include "shell/common/gin_helper/cleaned_up_at_exit.h"
#include "shell/common/gin_helper/pinnable.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace lok {
class Office;
class Document;
}  // namespace lok

typedef struct _UnoV8 UnoV8;

namespace electron::office {

class EventBus;
class DocumentClient;

class OfficeClient : public gin::Wrappable<OfficeClient>, OfficeLoadObserver {
 public:
  static constexpr char kGlobalEntry[] = "libreoffice";

  static void InstallToContext(v8::Local<v8::Context> context);
  static void RemoveFromContext(v8::Local<v8::Context> context);

  // disable copy
  OfficeClient(const OfficeClient&) = delete;
  OfficeClient& operator=(const OfficeClient&) = delete;

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // OfficeLoadObserver
  void OnLoaded(lok::Office* office) override;

  // v8 EventBus
  void On(const std::string& event_name,
          v8::Local<v8::Function> listener_callback);
  void Off(const std::string& event_name,
           v8::Local<v8::Function> listener_callback);
  void Emit(const std::string& event_name, v8::Local<v8::Value> data);

  lok::Office* GetOffice();

  DocumentClient* PrepareDocumentClient(lok::Document* document);

 protected:
  DocumentHolder LoadDocument(const std::string& path);
  // Exposed to v8 {
  std::string GetLastError();
  v8::Local<v8::Promise> SetDocumentPasswordAsync(v8::Isolate* isolate,
                                                  const std::string& url,
                                                  const std::string& password);
  // }
  v8::Local<v8::Promise> LoadDocumentAsync(v8::Isolate* isolate,
                                           const std::string& path);
  void LoadDocumentComplete(v8::Isolate* isolate,
                            ThreadedPromiseResolver resolver,
                            DocumentHolder docHolder);

  v8::Local<v8::Promise> LoadDocumentFromArrayBuffer(
      v8::Isolate* isolate,
      v8::Local<v8::ArrayBuffer> array_buffer);

 private:
  OfficeClient();
  ~OfficeClient() override;

  lok::Office* office_ = nullptr;
  std::unordered_map<std::string, lok::Document*> document_map_;
  std::unordered_set<lok::Document*> documents_mounted_;

  EventBus event_bus_;

  typedef struct _DocumentCallbackContext {
    _DocumentCallbackContext(
        scoped_refptr<base::SequencedTaskRunner> task_runner_,
        base::WeakPtr<DocumentClient> client_);
    ~_DocumentCallbackContext();

    scoped_refptr<base::SequencedTaskRunner> task_runner;
    base::WeakPtr<DocumentClient> client;
    base::AtomicFlag invalid;
  } DocumentCallbackContext;

  std::unordered_map<lok::Document*, DocumentCallbackContext*>
      document_contexts_;

  v8::Global<v8::Context> context_;
  base::OneShotEvent loaded_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<OfficeClient> weak_factory_{this};
};

}  // namespace electron::office
#endif  // OFFICE_OFFICE_CLIENT_H_
