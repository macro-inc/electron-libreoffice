// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/input_event.mojom-shared.h"

namespace electron::office::simulated_input {

constexpr int kKeyDown = (int)blink::mojom::EventType::kRawKeyDown;
constexpr int kKeyUp = (int)blink::mojom::EventType::kKeyUp;
constexpr int kKeyPress = kKeyDown | kKeyUp << 1;
constexpr int kMouseDown = (int)blink::mojom::EventType::kMouseDown;
constexpr int kMouseMove = (int)blink::mojom::EventType::kMouseMove;
constexpr int kMouseUp = (int)blink::mojom::EventType::kMouseUp;
constexpr int kMouseClick = kMouseDown | kMouseUp << 1;
constexpr int kLeft = blink::WebInputEvent::Modifiers::kLeftButtonDown;
constexpr int kRight = blink::WebInputEvent::Modifiers::kRightButtonDown;
constexpr int kMiddle = blink::WebInputEvent::Modifiers::kMiddleButtonDown;
constexpr int kBack = blink::WebInputEvent::Modifiers::kBackButtonDown;
constexpr int kForward = blink::WebInputEvent::Modifiers::kForwardButtonDown;

std::pair<int, std::string> ExtractModifiers(const std::string& input);
std::unique_ptr<blink::WebKeyboardEvent> TranslateKeyEvent(
    int type,
    const std::string& keys);
std::unique_ptr<blink::WebInputEvent> CreateMouseEvent(
    int type,
    int buttons,
    float x,
    float y,
    const std::string& modifiers);
gfx::PointF GetMousePosition(const blink::WebInputEvent& event);
int GetClickCount(const blink::WebInputEvent& event);

}  // namespace electron::office::simulated_input
