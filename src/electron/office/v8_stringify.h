// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include "v8/include/v8-context.h"
#include "v8/include/v8-value.h"
#include "v8/include/v8-local-handle.h"

namespace electron::office {

std::unique_ptr<char[]> v8_stringify(const v8::Local<v8::Context>& context,
                                  const v8::Local<v8::Value>& val);

}
