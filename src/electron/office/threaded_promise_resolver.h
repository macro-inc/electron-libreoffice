// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_THREADED_PROMISE_RESOLVER_H_
#define OFFICE_THREADED_PROMISE_RESOLVER_H_

#include "v8-weak-callback-info.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-promise.h"

namespace electron::office {

/**
 * A thread-friendly v8::Promise::Resolver wrapper.
 *
 * v8::Global is not thread safe and a race condition can occur when a task or a
 * task reply outlives the context. Since a v8::Promise is inherently async,
 * this case is very likely to occur in a ThreadRunner that is neither
 * single-threaded nor sequenced.
 */
class ThreadedPromiseResolver {
 public:
  ThreadedPromiseResolver(v8::Isolate* isolate,
                          v8::Local<v8::Promise::Resolver> resolver);
  // no copy
  explicit ThreadedPromiseResolver(const ThreadedPromiseResolver&) = delete;
  ThreadedPromiseResolver& operator=(const ThreadedPromiseResolver&) = delete;


  v8::Maybe<bool> Resolve(v8::Isolate* isolate, v8::Local<v8::Value> value);
  v8::Maybe<bool> Reject(v8::Isolate* isolate, v8::Local<v8::Value> value);
  v8::Local<v8::Context> GetCreationContext(v8::Isolate* isolate);

  ~ThreadedPromiseResolver();

 private:
  ThreadedPromiseResolver();

  bool IsValid();

  v8::Global<v8::Promise::Resolver> resolver_;
  v8::Global<v8::Context> context_;
};

}  // namespace electron::office

#endif  // OFFICE_THREADED_PROMISE_RESOLVER_H_
