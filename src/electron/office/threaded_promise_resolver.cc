// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "threaded_promise_resolver.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "office/async_scope.h"
#include "v8-maybe.h"

namespace electron::office {
ThreadedPromiseResolver::ThreadedPromiseResolver(
    v8::Isolate* isolate,
    v8::Local<v8::Promise::Resolver> resolver) {
  context_.Reset(isolate,
                 resolver->GetPromise()->GetCreationContext().ToLocalChecked());
  context_.AnnotateStrongRetainer(
      "office::ThreadedPromiseResolver::ThreadedPromiseResolver");
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

void ThreadedPromiseResolver::Resolve(v8::Local<v8::Value> value) {
  if (!IsValid()) {
    return;
  }
  const v8::Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  const v8::MicrotasksScope microtasks_scope(
      isolate_, v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (!IsValid()) {
    return;
  }
  v8::Local<v8::Context> context = GetCreationContext();
  v8::Local<v8::Promise::Resolver> resolver = resolver_.Get(isolate_);

  if (!IsValid()) {
    return;
  }

  base::IgnoreResult(resolver->Resolve(context, value));
}

void ThreadedPromiseResolver::Reject(v8::Local<v8::Value> value) {
  if (!IsValid())
    return;
  const v8::Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  const v8::MicrotasksScope microtasks_scope(
      isolate_, v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (!IsValid())
    return;
  v8::Local<v8::Context> context = GetCreationContext();
  v8::Local<v8::Promise::Resolver> resolver = resolver_.Get(isolate_);

  if (!IsValid())
    return;

  base::IgnoreResult(resolver->Reject(context, value));
}

v8::Local<v8::Context> ThreadedPromiseResolver::GetCreationContext() {
  return v8::Local<v8::Context>::New(isolate_, context_);
}

}  // namespace electron::office
