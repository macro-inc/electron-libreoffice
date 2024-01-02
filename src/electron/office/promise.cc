// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "promise.h"
#include "base/bind.h"
#include "v8/include/v8-exception.h"

namespace electron::office {

PromiseBase::PromiseBase(v8::Isolate* isolate,
                         scoped_refptr<base::SequencedTaskRunner> runner)
    : PromiseBase(isolate,
                  v8::Promise::Resolver::New(isolate->GetCurrentContext())
                      .ToLocalChecked(),
                  std::move(runner)) {}

PromiseBase::PromiseBase(v8::Isolate* isolate,
                         v8::Local<v8::Promise::Resolver> handle,
                         scoped_refptr<base::SequencedTaskRunner> runner)

    : isolate_(isolate),
      context_(isolate, isolate->GetCurrentContext()),
      resolver_(isolate, handle),
      task_runner_(runner) {}

PromiseBase::PromiseBase(PromiseBase&&) = default;
PromiseBase& PromiseBase::operator=(PromiseBase&&) = default;

PromiseBase::~PromiseBase() = default;

v8::Maybe<bool> PromiseBase::Reject() {
  v8::HandleScope handle_scope(isolate());
  const v8::MicrotasksScope microtasks_scope(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(GetContext());

  return resolver()->Reject(GetContext(), v8::Undefined(isolate()));
}

v8::Maybe<bool> PromiseBase::Resolve() {
  v8::HandleScope handle_scope(isolate());
  const v8::MicrotasksScope microtasks_scope(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(GetContext());

  return resolver()->Resolve(GetContext(), v8::Undefined(isolate()));
}

v8::Maybe<bool> PromiseBase::Reject(v8::Local<v8::Value> except) {
  v8::HandleScope handle_scope(isolate());
  const v8::MicrotasksScope microtasks_scope(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(GetContext());

  return resolver()->Reject(GetContext(), except);
}

v8::Maybe<bool> PromiseBase::RejectWithErrorMessage(base::StringPiece message) {
  v8::HandleScope handle_scope(isolate());
  const v8::MicrotasksScope microtasks_scope(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(GetContext());

  v8::Local<v8::Value> error =
      v8::Exception::Error(gin::StringToV8(isolate(), message));
  return resolver()->Reject(GetContext(), (error));
}

v8::Local<v8::Context> PromiseBase::GetContext() const {
  return v8::Local<v8::Context>::New(isolate_, context_);
}

v8::Local<v8::Promise> PromiseBase::GetHandle() const {
  return resolver()->GetPromise();
}

v8::Local<v8::Promise::Resolver> PromiseBase::resolver() const {
  return resolver_.Get(isolate());
}

scoped_refptr<base::SequencedTaskRunner> PromiseBase::task_runner() const {
  return task_runner_;
}

// static
void Promise<void>::ResolvePromise(Promise<void> promise) {
  promise.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce([](Promise<void> promise) { promise.Resolve(); },
                     std::move(promise)));
}

// static
v8::Local<v8::Promise> Promise<void>::ResolvedPromise(v8::Isolate* isolate) {
  Promise<void> resolved(isolate);
  resolved.Resolve();
  return resolved.GetHandle();
}

void Promise<v8::Value>::Resolve() {
  Resolve(v8::Undefined(isolate()));
}

void Promise<v8::Value>::Resolve(v8::Local<v8::Value> value) {
  std::ignore = resolver()->Resolve(GetContext(), value);
}

// static
void Promise<v8::Value>::ResolvePromise(Promise<v8::Value> promise,
                                        v8::Global<v8::Value> result) {
  promise.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Promise<v8::Value> promise, v8::Global<v8::Value> result) {
            promise.Resolve(result.Get(promise.isolate()));
          },
          std::move(promise), std::move(result)));
}

// static
void Promise<v8::Value>::ResolvePromise(Promise<v8::Value> promise) {
  promise.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce([](Promise<v8::Value> promise) { promise.Resolve(); },
                     std::move(promise)));
}

}  // namespace electron::office
