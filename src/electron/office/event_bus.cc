// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/event_bus.h"

#include <algorithm>
#include <unordered_map>
#include "base/bind.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/document_client.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "shell/common/gin_helper/dictionary.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

namespace electron::office {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;

gin::WrapperInfo EventBus::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder EventBus::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> constructor =
      data->GetFunctionTemplate(&this->kWrapperInfo);
  if (constructor.IsEmpty()) {
    constructor = v8::FunctionTemplate::New(isolate);
    constructor->SetClassName(gin::StringToV8(isolate, GetTypeName()));
    data->SetFunctionTemplate(&this->kWrapperInfo, constructor);
  }
  return Extend(gin::ObjectTemplateBuilder(isolate, GetTypeName(),
                                           constructor->InstanceTemplate()));
}

const char* EventBus::GetTypeName() {
  return "EventBus";
}

void EventBus::On(const std::string& event_name,
                  v8::Local<v8::Function> listener_callback) {
  if (listener_callback.IsEmpty())
    return;

  event_listeners_.try_emplace(event_name, std::vector<PersistedFn>());

  PersistedFn persisted(listener_callback->GetIsolate(), listener_callback);
  event_listeners_[event_name].push_back(persisted);
}

void EventBus::Off(const std::string& event_name,
                   v8::Local<v8::Function> listener_callback) {
  auto itr = event_listeners_.find(event_name);
  if (itr == event_listeners_.end())
    return;

  auto& vec = itr->second;
  vec.erase(std::remove(vec.begin(), vec.end(), listener_callback), vec.end());
}

void EventBus::Handle(LibreOfficeKitCallbackType type, EventCallback callback) {
  internal_event_listeners_[type].emplace_back(callback);
}

void EventBus::Emit(const std::string& event_name, v8::Local<v8::Value> data) {
  auto itr = event_listeners_.find(event_name);
  if (itr == event_listeners_.end())
    return;

  auto& vec = itr->second;
  if (vec.empty())
    return;

  v8::Local<v8::Value> args[] = {data};
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  for (PersistedFn& callback : vec) {
    if (std::move(callback).IsEmpty())
      continue;

    v8::HandleScope handle_scope(isolate);
    auto local = v8::Local<v8::Function>::New(isolate, std::move(callback));
    auto context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    std::ignore = local->Call(context, v8::Null(isolate), 1, args);
  }
}

void EventBus::EmitLibreOfficeEvent(int type, std::string payload) {
  LibreOfficeKitCallbackType type_enum =
      static_cast<LibreOfficeKitCallbackType>(type);
  // handle internal events first
  auto internal = internal_event_listeners_.find(type_enum);
  if (internal != internal_event_listeners_.end() &&
      !internal->second.empty()) {
    for (EventCallback& callback : internal->second) {
      callback.Run(payload);
    }
  }

  std::string type_string = lok_callback::TypeToEventString(type);
  if (event_listeners_.find(type_string) == event_listeners_.end())
    return;

  // TODO: Is there a better way of getting the isolate?
  v8::Isolate* isolate = blink::MainThreadIsolate();

  // coming from another thread means the original scope is entirely lost
  v8::HandleScope handle_scope(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Handle<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
  dict.Set("type", type_string);
  dict.Set("payload",
           lok_callback::PayloadToLocalValue(isolate, type, payload.c_str()));

  Emit(type_string, dict.GetHandle());
}

base::WeakPtr<EventBus> EventBus::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

gin::ObjectTemplateBuilder EventBus::Extend(
    gin::ObjectTemplateBuilder builder) {
  return builder
      .SetMethod("on",
                 base::BindRepeating(&EventBus::On, base::Unretained(this)))
      .SetMethod("off",
                 base::BindRepeating(&EventBus::Off, base::Unretained(this)))
      .SetMethod("emit",
                 base::BindRepeating(&EventBus::Emit, base::Unretained(this)));
}

}  // namespace electron::office
