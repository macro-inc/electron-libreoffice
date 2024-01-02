// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// This is a modification of the Promise wrapper used in the Electron shell to
// make it useful for other usecases

#pragma once

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gin/wrappable.h"
#include "shell/common/gin_converters/std_converter.h"
#include "v8-primitive.h"
#include "v8/include/v8-microtask-queue.h"

namespace electron::office {

// A wrapper around the v8::Promise.
//
// This is the non-template base class to share code between templates
// instances.
//
// This is a move-only type that should always be `std::move`d when passed to
// callbacks, and it should be destroyed on the same thread of creation.
class PromiseBase {
 public:
  explicit PromiseBase(v8::Isolate* isolate,
                       scoped_refptr<base::SequencedTaskRunner> runner =
                           base::SequencedTaskRunnerHandle::Get());
  PromiseBase(v8::Isolate* isolate,
              v8::Local<v8::Promise::Resolver> handle,
              scoped_refptr<base::SequencedTaskRunner> runner =
                  base::SequencedTaskRunnerHandle::Get());
  ~PromiseBase();

  // disable copy
  PromiseBase(const PromiseBase&) = delete;
  PromiseBase& operator=(const PromiseBase&) = delete;

  // Support moving.
  PromiseBase(PromiseBase&&);
  PromiseBase& operator=(PromiseBase&&);

  // Helper for rejecting promise with error message.
  //
  // Note: The parameter type is PromiseBase&& so it can take the instances of
  // Promise<T> type.
  static void RejectPromise(PromiseBase&& promise, base::StringPiece errmsg) {
    promise.task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](PromiseBase&& promise, std::string str) {
              promise.RejectWithErrorMessage(str);
            },
            std::move(promise), std::string(errmsg.data(), errmsg.size())));
  }

  v8::Maybe<bool> Reject();
  v8::Maybe<bool> Reject(v8::Local<v8::Value> except);
  v8::Maybe<bool> RejectWithErrorMessage(base::StringPiece message);
  v8::Maybe<bool> Resolve();

  v8::Local<v8::Context> GetContext() const;
  v8::Local<v8::Promise> GetHandle() const;

  v8::Isolate* isolate() const { return isolate_; }
  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

 protected:
  v8::Local<v8::Promise::Resolver> resolver() const;

 private:
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  v8::Global<v8::Promise::Resolver> resolver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// Template implementation that returns values.
template <typename RT>
class Promise : public PromiseBase {
 public:
  using PromiseBase::PromiseBase;

  // Helper for resolving the promise with |result|.
  static void ResolvePromise(Promise<RT> promise, RT result) {
    promise.task_runner()->PostTask(
        FROM_HERE, base::BindOnce([](Promise<RT> promise,
                                     RT result) { promise.Resolve(result); },
                                  std::move(promise), std::move(result)));
  }

  static void ResolvePromise(Promise<RT> promise) {
    promise.task_runner()->PostTask(
        FROM_HERE, base::BindOnce([](Promise<RT> promise,
                                     RT result) { promise.Resolve(); },
                                  std::move(promise)));
  }

  // Returns an already-resolved promise.
  static v8::Local<v8::Promise> ResolvedPromise(v8::Isolate* isolate,
                                                RT result) {
    Promise<RT> resolved(isolate);
    resolved.Resolve(result);
    return resolved.GetHandle();
  }

  // Convert to another type.
  template <typename NT>
  Promise<NT> As() {
    return Promise<NT>(isolate(), resolver());
  }

  // Promise resolution is a microtask
  // We use the MicrotasksRunner to trigger the running of pending microtasks
  v8::Maybe<bool> Resolve(const RT& value) {
    v8::HandleScope handle_scope(isolate());
    const v8::MicrotasksScope microtasks_scope(
        isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Context::Scope context_scope(GetContext());

    return resolver()->Resolve(GetContext(),
                               gin::ConvertToV8(isolate(), value));
  }

  void Resolve() { PromiseBase::Resolve(); }

  // A very common case of converting a gin::Wrappable
  template <typename P = RT,
            typename = std::enable_if_t<
                std::is_pointer<P>::value &&
                std::is_base_of<gin::Wrappable<std::remove_pointer_t<P>>,
                                std::remove_pointer_t<P>>::value>>
  void Resolve(P value) {
    v8::HandleScope handle_scope(isolate());
    const v8::MicrotasksScope microtasks_scope(
        isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Context::Scope context_scope(GetContext());
    v8::Local<v8::Value> result;
    // nullptr
    if (!value)
      Resolve();

    if (!value->GetWrapper(isolate()).ToLocal(&result)) {
      // Conversion failure
      Resolve();
    }

    std::ignore = resolver()->Resolve(GetContext(), result);
  }
};

// Template implementation that returns nothing.
template <>
class Promise<void> : public PromiseBase {
 public:
  using PromiseBase::PromiseBase;

  // Helper for resolving the empty promise.
  static void ResolvePromise(Promise<void> promise);

  // Returns an already-resolved promise.
  static v8::Local<v8::Promise> ResolvedPromise(v8::Isolate* isolate);
};

// Template implementation that handles v8::Value
template <>
class Promise<v8::Value> : public PromiseBase {
 public:
  using PromiseBase::PromiseBase;

  void Resolve();
  void Resolve(v8::Local<v8::Value> value);
  static void ResolvePromise(Promise<v8::Value> promise,
                             v8::Global<v8::Value> result);
  static void ResolvePromise(Promise<v8::Value> promise);
};

}  // namespace electron::office

namespace gin {

template <typename T>
struct Converter<electron::office::Promise<T>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const electron::office::Promise<T>& val) {
    return val.GetHandle();
  }
};

}  // namespace gin

