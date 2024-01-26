// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/task/sequenced_task_runner.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office_load_observer.h"
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

class OfficeClient : public gin::Wrappable<OfficeClient>,
                     public OfficeLoadObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static constexpr char kGlobalEntry[] = "libreoffice";
  OfficeClient();
  ~OfficeClient() override;

  static base::WeakPtr<OfficeClient> GetWeakPtr();
  static void InstallToContext(v8::Local<v8::Context> context);
  static void RemoveFromContext(v8::Local<v8::Context> context);

  // disable copy
  OfficeClient(const OfficeClient&) = delete;
  OfficeClient& operator=(const OfficeClient&) = delete;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // OfficeLoadObserver
  void OnLoaded(lok::Office* office) override;

  lok::Office* GetOffice() const;
  v8::Local<v8::Value> GetHandle(v8::Isolate* isolate);
  void Unset();
  void HandleBeforeUnload();

 protected:
  // Exposed to v8 {
  std::string GetLastError();
	// TODO: [MACRO-1899] fix setDocumentPassword in LOK, then re-enable
	/*
  v8::Local<v8::Promise> SetDocumentPasswordAsync(v8::Isolate* isolate,
                                                  v8::Local<v8::Value> url,
                                                  v8::Local<v8::Value> maybePassword);
	*/
  v8::Local<v8::Promise> LoadDocumentAsync(v8::Isolate* isolate,
                                           v8::Local<v8::Value> url);
  v8::Local<v8::Promise> LoadDocumentFromArrayBuffer(
      v8::Isolate* isolate,
      v8::Local<v8::ArrayBuffer> array_buffer);
  // }

 private:
  lok::Office* office_ = nullptr;

  v8::Global<v8::Context> context_;
  v8::Global<v8::Value> self_;
  base::OneShotEvent loaded_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<OfficeClient> weak_factory_{this};
};

}  // namespace electron::office
