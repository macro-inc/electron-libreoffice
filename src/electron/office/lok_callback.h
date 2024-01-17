// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"

namespace electron::office::lok_callback {

std::string TypeToEventString(int type);
int EventStringToType(const std::u16string& event_string);
bool IsTypeJSON(int type);
bool IsTypeCSV(int type);
bool IsTypeMultipleCSV(int type);

std::vector<uint64_t> ParseCSV(std::string_view::const_iterator& target,
                               std::string_view::const_iterator end);
std::vector<std::vector<uint64_t>> ParseMultipleCSV(
    std::string_view::const_iterator& target,
    std::string_view::const_iterator end);

gfx::Rect ParseRect(std::string_view::const_iterator& target,
                    std::string_view::const_iterator end);
std::vector<gfx::Rect> ParseMultipleRects(
    std::string_view::const_iterator& target,
    std::string_view::const_iterator end,
    size_t size);

v8::Local<v8::Value> ParseJSON(v8::Isolate* isolate,
                               v8::Local<v8::String> json);
v8::Local<v8::Value> PayloadToLocalValue(v8::Isolate* isolate,
                                         int type,
                                         const char* payload);

constexpr float kTwipPerPx = 15.0f;
inline float PixelToTwip(float in, float zoom) {
  return in / zoom * kTwipPerPx;
}

inline float TwipToPixel(float in, float zoom) {
  return in / kTwipPerPx * zoom;
}

}  // namespace electron::office::lok_callback

