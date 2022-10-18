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
#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office/event_bus.h"
#include "third_party/libreofficekit/LibreOfficeKit.hxx"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace electron::office {

class OfficeClient : public gin::Wrappable<OfficeClient> {
 public:
  static constexpr char kGlobalEntry[] = "libreoffice";

  static OfficeClient* GetInstance();
  static void HandleLibreOfficeCallback(int type,
                                        const char* payload,
                                        void* office_client);
  static void HandleDocumentCallback(int type,
                                     const char* payload,
                                     void* document);
  void HandleDocumentEvent(lok::Document* document,
                           LibreOfficeKitCallbackType type,
                           EventBus::EventCallback callback);
  static gin::Handle<OfficeClient> GetHandle(v8::Isolate* isolate);
  static bool IsValid();

  // disable copy
  OfficeClient(const OfficeClient&) = delete;
  OfficeClient& operator=(const OfficeClient&) = delete;

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  lok::Office* GetOffice();
  lok::Document* GetDocument(const std::string& path);
  lok::Document* InitializeOnce();

  bool MarkMounted(lok::Document* document);

 protected:
  std::string GetLastError();
  v8::Local<v8::Value> LoadDocument(v8::Isolate* isolate,
                                    const std::string& path);
  bool CloseDocument(const std::string& path);

 private:
  OfficeClient();
  ~OfficeClient() override;

  void EmitLibreOfficeEvent(int type, const char* payload);

  friend struct base::DefaultSingletonTraits<OfficeClient>;

  lok::Office* office_ = nullptr;
  std::unordered_map<std::string, lok::Document*> document_map_;
  std::unordered_set<lok::Document*> documents_mounted_;
  std::unordered_map<lok::Document*, EventBus*> document_event_router_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ = nullptr;
  EventBus event_bus_;
};

}  // namespace electron::office
#endif  // OFFICE_OFFICE_CLIENT_H_
