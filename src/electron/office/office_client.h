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
#include "base/lazy_instance.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_local.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office/event_bus.h"
#include "office/threaded_promise_resolver.h"
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

class OfficeClient : public gin::Wrappable<OfficeClient> {
 public:
  static constexpr char kGlobalEntry[] = "libreoffice";

  static OfficeClient* GetCurrent();
  static bool IsValid();

  static void HandleLibreOfficeCallback(int type,
                                        const char* payload,
                                        void* office_client);
  static void HandleDocumentCallback(int type,
                                     const char* payload,
                                     void* document);
  static const ::UnoV8& GetUnoV8();

  // disable copy
  OfficeClient(const OfficeClient&) = delete;
  OfficeClient& operator=(const OfficeClient&) = delete;

  v8::Local<v8::Object> GetHandle(v8::Local<v8::Context> context);
  void InstallToContext(v8::Local<v8::Context> context);
  void RemoveFromContext(v8::Local<v8::Context> context);

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // v8 EventBus
  void On(const std::string& event_name,
          v8::Local<v8::Function> listener_callback);
  void Off(const std::string& event_name,
           v8::Local<v8::Function> listener_callback);
  void Emit(const std::string& event_name, v8::Local<v8::Value> data);

  lok::Office* GetOffice();
  lok::Document* GetDocument(const std::string& path);

  bool MarkMounted(lok::Document* document);
  bool CloseDocument(const std::string& path);

  // Exposed to v8 {
  v8::Local<v8::Value> GetFilterTypes(gin::Arguments* args);
  void SetDocumentPassword(const std::string& url, const std::string& password);
  v8::Local<v8::Value> GetVersionInfo(gin::Arguments* args);
  void SendDialogEvent(uint64_t window_id, gin::Arguments* args);
  bool RunMacro(const std::string& url);
  // }

  typedef std::pair<lok::Document*, int> LOKDocWithViewId;
  DocumentClient* PrepareDocumentClient(LOKDocWithViewId doc,
                                        const std::string& path);

 protected:
  std::string GetLastError();
  LOKDocWithViewId LoadDocument(const std::string& path);
  v8::Local<v8::Promise> LoadDocumentAsync(v8::Isolate* isolate,
                                           const std::string& path);
  void LoadDocumentComplete(v8::Isolate* isolate,
                            ThreadedPromiseResolver* resolver,
                            const std::string& path,
                            LOKDocWithViewId client);
  v8::Local<v8::Value> LoadDocumentFromArrayBuffer(v8::Isolate* isolate, v8::Local<v8::ArrayBuffer> array_buffer);

 private:
  OfficeClient();
  ~OfficeClient() override;

  void EmitLibreOfficeEvent(int type, const char* payload);
  void Destroy();
  bool destroyed_ = false;

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

  // prevents this global from being released until the isolate is destroyed
  v8::Eternal<v8::Object> eternal_;

  base::WeakPtrFactory<OfficeClient> weak_factory_{this};
};

}  // namespace electron::office
#endif  // OFFICE_OFFICE_CLIENT_H_
