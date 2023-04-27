// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/event_bus.h"

#include <algorithm>
#include <unordered_map>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "async_scope.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "office/document_client.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "shell/common/gin_helper/dictionary.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

namespace electron::office {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;

void EventBus::On(const std::string& event_name,
                  v8::Local<v8::Function> listener_callback) {
  if (listener_callback.IsEmpty())
    return;

  event_listeners_.try_emplace(event_name, std::vector<PersistedFn>());

  PersistedFn persisted(listener_callback->GetIsolate(), listener_callback);
  event_listeners_[event_name].emplace_back(std::move(persisted));
}

void EventBus::Off(const std::string& event_name,
                   v8::Local<v8::Function> listener_callback) {
  auto itr = event_listeners_.find(event_name);
  if (itr == event_listeners_.end())
    return;

  auto& vec = itr->second;
  vec.erase(std::remove(vec.begin(), vec.end(), listener_callback), vec.end());
}

void EventBus::Handle(int type, EventCallback callback) {
  internal_event_listeners_[type].emplace_back(callback);
}

void EventBus::Emit(const std::string& event_name, v8::Local<v8::Value> data) {
  if (context_.IsEmpty())
    return;

  auto itr = event_listeners_.find(event_name);
  if (itr == event_listeners_.end())
    return;

  auto& vec = itr->second;
  if (vec.empty())
    return;

  v8::Local<v8::Value> args[] = {data};

  v8::TryCatch try_catch(isolate_);
  for (PersistedFn& callback : vec) {
    if (callback.IsEmpty())
      continue;

    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Function> fn =
        v8::Local<v8::Function>::New(isolate_, std::move(callback));
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> discarded;
    if (!fn->Call(context, v8::Null(isolate_), 1, args).ToLocal(&discarded)) {
      DCHECK(try_catch.HasCaught());
      try_catch.Reset();
    }
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
  if (event_listeners_.find(type_string) == event_listeners_.end()) {
    return;
  }

  if (context_.IsEmpty() || !isolate_) {
    LOG(ERROR) << "CONTEXT IS INVALID";
    return;
  }

  // coming from another thread means the original scope is entirely lost
  AsyncScope async_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate_);
  dict.Set("type", type_string);
  dict.Set("payload",
           lok_callback::PayloadToLocalValue(isolate_, type, payload.c_str()));

  Emit(type_string, dict.GetHandle());
}

base::WeakPtr<EventBus> EventBus::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EventBus::SetContext(v8::Isolate* isolate,
                          v8::Local<v8::Context> context) {
  isolate_ = isolate;
  context_.Reset(isolate, context);
  context_.AnnotateStrongRetainer("office::EventBus::context_");
}

}  // namespace electron::office
