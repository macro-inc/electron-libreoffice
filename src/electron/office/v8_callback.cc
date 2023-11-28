// Copyright (c) 2019 GitHub, Inc. All rights reserved.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "v8_callback.h"

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gin/dictionary.h"

namespace electron::office {

SafeV8Function::SafeV8Function(v8::Isolate* isolate,
                               v8::Local<v8::Value> value)
    : v8_function_(
          base::MakeRefCounted<RefCountedGlobal<v8::Function>>(isolate,
                                                               value)) {}

SafeV8Function::SafeV8Function(const SafeV8Function& other) = default;

SafeV8Function::~SafeV8Function() = default;

bool SafeV8Function::IsAlive() const {
  return v8_function_.get() && v8_function_->IsAlive();
}

v8::Local<v8::Function> SafeV8Function::NewHandle(v8::Isolate* isolate) const {
  return v8_function_->NewHandle(isolate);
}

bool SafeV8Function::operator==(const v8::Local<v8::Function>& other) const {
  return *v8_function_ == other;
}

}  // namespace electron::office
