// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "async_resolver_scope.h"

namespace electron::office {
AsyncResolverScope::AsyncResolverScope(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> resolver)
    : isolate_scope_(isolate),
      handle_scope_(isolate),
      microtasks_scope_(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks),
      resolver_(resolver.Get(isolate)),
      context_scope_(resolver_->GetCreationContextChecked()) {}

AsyncResolverScope::~AsyncResolverScope() = default;

v8::Local<v8::Promise::Resolver> AsyncResolverScope::Resolver() {
  return resolver_;
}

}  // namespace electron::office
