// Copyright (c) 2019 GitHub, Inc. All rights reserved.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gin/dictionary.h"
#include "shell/common/gin_converters/std_converter.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"

namespace electron::office {

// Like v8::Global, but ref-counted.
template <typename T>
class RefCountedGlobal
    : public base::RefCountedDeleteOnSequence<RefCountedGlobal<T>> {
 public:
  RefCountedGlobal(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : base::RefCountedDeleteOnSequence<RefCountedGlobal<T>>(
            base::SequencedTaskRunnerHandle::Get()),
        handle_(isolate, value.As<T>()) {}

  bool IsAlive() const { return !handle_.IsEmpty(); }

  v8::Local<T> NewHandle(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, handle_);
  }

  bool operator==(const v8::Local<T>& that) const { return handle_ == that; }

 private:
  v8::Global<T> handle_;
};

// Manages the V8 function with RAII.
class SafeV8Function {
 public:
  SafeV8Function(v8::Isolate* isolate, v8::Local<v8::Value> value);
  SafeV8Function(const SafeV8Function& other);
  ~SafeV8Function();

  bool IsAlive() const;
  v8::Local<v8::Function> NewHandle(v8::Isolate* isolate) const;
  bool operator==(const v8::Local<v8::Function>& other) const;

 private:
  scoped_refptr<RefCountedGlobal<v8::Function>> v8_function_;
};

// Helper to invoke a V8 function with C++ parameters.
template <typename Sig>
struct V8FunctionInvoker {};

template <typename... ArgTypes>
struct V8FunctionInvoker<v8::Local<v8::Value>(ArgTypes...)> {
  static v8::Local<v8::Value> Go(v8::Isolate* isolate,
                                 const SafeV8Function& function,
                                 ArgTypes... raw) {
    v8::EscapableHandleScope handle_scope(isolate);
    if (!function.IsAlive())
      return v8::Undefined(isolate);
		v8::MicrotasksScope microtasks_scope(
			isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> holder = function.NewHandle(isolate);
    v8::Local<v8::Context> context = holder->GetCreationContextChecked();
    v8::Context::Scope context_scope(context);
    std::vector<v8::Local<v8::Value>> args{
        gin::ConvertToV8(isolate, std::forward<ArgTypes>(raw))...};
    v8::TryCatch try_catch(isolate);
    v8::MaybeLocal<v8::Value> ret = holder->Call(
        context, holder, args.size(), args.empty() ? nullptr : &args.front());
    try_catch.Reset();
    if (ret.IsEmpty())
      return v8::Undefined(isolate);
    else
      return handle_scope.Escape(ret.ToLocalChecked());
  }
};

template <typename... ArgTypes>
struct V8FunctionInvoker<void(ArgTypes...)> {
  static void Go(v8::Isolate* isolate,
                 const SafeV8Function& function,
                 ArgTypes... raw) {
    v8::HandleScope handle_scope(isolate);
    if (!function.IsAlive())
      return;
		v8::MicrotasksScope microtasks_scope(
			isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> holder = function.NewHandle(isolate);
    v8::Local<v8::Context> context = holder->GetCreationContextChecked();
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate);
    std::vector<v8::Local<v8::Value>> args{
        gin::ConvertToV8(isolate, std::forward<ArgTypes>(raw))...};
    holder
        ->Call(context, holder, args.size(),
               args.empty() ? nullptr : &args.front())
        .IsEmpty();
    try_catch.Reset();
  }
};

template <typename ReturnType, typename... ArgTypes>
struct V8FunctionInvoker<ReturnType(ArgTypes...)> {
  static ReturnType Go(v8::Isolate* isolate,
                       const SafeV8Function& function,
                       ArgTypes... raw) {
    v8::HandleScope handle_scope(isolate);
    ReturnType ret = ReturnType();
    if (!function.IsAlive())
      return ret;
		v8::MicrotasksScope microtasks_scope(
			isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> holder = function.NewHandle(isolate);
    v8::Local<v8::Context> context = holder->GetCreationContextChecked();
    v8::Context::Scope context_scope(context);
    std::vector<v8::Local<v8::Value>> args{
        gin::ConvertToV8(isolate, std::forward<ArgTypes>(raw))...};
    v8::Local<v8::Value> result;
    v8::TryCatch try_catch(isolate);
    auto maybe_result = holder->Call(context, holder, args.size(),
                                     args.empty() ? nullptr : &args.front());
    try_catch.Reset();
    if (maybe_result.ToLocal(&result))
      gin::Converter<ReturnType>::FromV8(isolate, result, &ret);
    return ret;
  }
};

}  // namespace electron::office

