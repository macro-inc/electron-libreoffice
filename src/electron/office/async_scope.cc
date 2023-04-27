// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "async_scope.h"

namespace electron::office {
AsyncScope::AsyncScope(v8::Isolate* isolate)
    : isolate_scope_(isolate),
      handle_scope_(isolate),
      microtasks_scope_(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks) {}

AsyncScope::~AsyncScope() = default;

}  // namespace electron::office
