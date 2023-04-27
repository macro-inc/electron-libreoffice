// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_ASYNC_RESOLVER_SCOPE_H_
#define OFFICE_ASYNC_RESOLVER_SCOPE_H_

#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-context.h"

namespace electron::office {

/**
 * Manages V8 scope for a promise that resolves across threads
 * Necessary because all scopes are lost across thread boundaries
 */
class AsyncScope {
 public:
  explicit AsyncScope(v8::Isolate* isolate);
  explicit AsyncScope(const AsyncScope&) = delete;
  AsyncScope& operator=(const AsyncScope&) = delete;
  ~AsyncScope();

 private:
  const v8::Isolate::Scope isolate_scope_;
  const v8::HandleScope handle_scope_;
  const v8::MicrotasksScope microtasks_scope_;
};

}

#endif  // OFFICE_ASYNC_RESOLVER_SCOPE_H_
