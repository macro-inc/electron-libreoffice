// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_EVENT_BUS_H_
#define OFFICE_EVENT_BUS_H_

#include <unordered_map>
#include <vector>
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "gin/object_template_builder.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "third_party/libreofficekit/LibreOfficeKitEnums.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"

namespace electron::office {

class DocumentClient;

class EventBus : public gin::Wrappable<EventBus> {
 public:
  EventBus();
  ~EventBus() override;

  // disable copy
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  typedef base::RepeatingCallback<void(std::string payload)> EventCallback;

  void On(const std::string& event_name,
          v8::Local<v8::Function> listener_callback);
  void Off(const std::string& event_name,
           v8::Local<v8::Function> listener_callback);
  void Handle(LibreOfficeKitCallbackType type, EventCallback callback);

  void Emit(const std::string& event_name, v8::Local<v8::Value> data);

  void EmitLibreOfficeEvent(int type, std::string payload);

  base::WeakPtr<EventBus> GetWeakPtr();

  gin::ObjectTemplateBuilder Extend(gin::ObjectTemplateBuilder builder);

  void SetContext(v8::Isolate* isolate, v8::Local<v8::Context> context);

 private:
  typedef v8::Global<v8::Function> PersistedFn;
  v8::Global<v8::Context> context_;
  raw_ptr<v8::Isolate> isolate_;

  std::unordered_map<std::string, std::vector<PersistedFn>> event_listeners_;
  std::unordered_map<LibreOfficeKitCallbackType, std::vector<EventCallback>>
      internal_event_listeners_;

  base::WeakPtrFactory<EventBus> weak_factory_{this};
};

}  // namespace electron::office
#endif  // OFFICE_EVENT_BUS_H_
