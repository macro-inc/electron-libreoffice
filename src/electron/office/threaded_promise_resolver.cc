// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "threaded_promise_resolver.h"
#include "office/async_scope.h"
#include "base/logging.h"
#include "v8-maybe.h"

namespace electron::office {
ThreadedPromiseResolver::ThreadedPromiseResolver(
    v8::Isolate* isolate,
    v8::Local<v8::Promise::Resolver> resolver) {
  context_.Reset(isolate, resolver->GetPromise()->GetCreationContext().ToLocalChecked());
  context_.AnnotateStrongRetainer("office::ThreadedPromiseResolver::ThreadedPromiseResolver");
  resolver_.Reset(isolate, std::move(resolver));
}

ThreadedPromiseResolver::ThreadedPromiseResolver() = default;

ThreadedPromiseResolver::~ThreadedPromiseResolver() {
  resolver_.Reset();
  context_.Reset();
}

bool ThreadedPromiseResolver::IsValid() {
  return !resolver_.IsEmpty() && !context_.IsEmpty();
}

v8::Maybe<bool> ThreadedPromiseResolver::Resolve(v8::Isolate* isolate,
                                                 v8::Local<v8::Value> value) {
  if (!IsValid()) {
    return v8::Nothing<bool>();
  }
  const v8::Isolate::Scope isolate_scope(isolate);
  const v8::HandleScope handle_scope(isolate);
  const v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (!IsValid()) {
    return v8::Nothing<bool>();
  }
  v8::Local<v8::Context> context = GetCreationContext(isolate);
  v8::Local<v8::Promise::Resolver> resolver = resolver_.Get(isolate);

  if (!IsValid()) {
    return v8::Nothing<bool>();
  }

  return resolver->Resolve(context, value);
}

v8::Maybe<bool> ThreadedPromiseResolver::Reject(v8::Isolate* isolate,
                                                v8::Local<v8::Value> value) {
  if (!IsValid())
    return v8::Nothing<bool>();
  const v8::Isolate::Scope isolate_scope(isolate);
  const v8::HandleScope handle_scope(isolate);
  const v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (!IsValid())
    return v8::Nothing<bool>();
  v8::Local<v8::Context> context = GetCreationContext(isolate);
  v8::Local<v8::Promise::Resolver> resolver = resolver_.Get(isolate);

  if (!IsValid())
    return v8::Nothing<bool>();

  return resolver->Reject(context, value);
}

v8::Local<v8::Context> ThreadedPromiseResolver::GetCreationContext(v8::Isolate* isolate)
{
  return v8::Local<v8::Context>::New(isolate, context_);
}

}  // namespace electron::office
