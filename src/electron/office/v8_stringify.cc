// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "v8_stringify.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"

namespace electron::office {

std::unique_ptr<char[]> v8_stringify(const v8::Local<v8::Context>& context,
                                     const v8::Local<v8::Value>& val) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Context::Scope context_scope(context);
  v8::HandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> str;
  if (val->IsNullOrUndefined())
    return {};

  if (!val->ToString(context).ToLocal(&str))
    return {};

  uint32_t len = str->Utf8Length(isolate);
  char* buf = new (std::nothrow) char[len + 1];
  if (!buf)
    return {};
  std::unique_ptr<char[]> ptr(buf);
  str->WriteUtf8(isolate, buf);
  buf[len] = '\0';

  return ptr;
}

}  // namespace electron::office
