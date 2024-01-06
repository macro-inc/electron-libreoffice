// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "simulated_input.h"
#include <memory>
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "electron/shell/common/keyboard_util.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace electron::office::simulated_input {
std::pair<int, std::string> ExtractModifiers(const std::string& input) {
  std::istringstream iss(base::ToLowerASCII(input));
  std::string key;
  int modifiers = 0;
  while (std::getline(iss, key, '+')) {
    if (key == "ctrl") {
      modifiers |= blink::WebInputEvent::kControlKey;
    } else if (key == "cmd" || key == "win" || key == "meta") {
      modifiers |= blink::WebInputEvent::kMetaKey;
    } else if (key == "shift") {
      modifiers |= blink::WebInputEvent::kShiftKey;
    } else if (key == "alt") {
      modifiers |= blink::WebInputEvent::kAltKey;
    } else if (key == "mod") {
#if BUILDFLAG(IS_MAC)
      modifiers |= blink::WebInputEvent::kMetaKey;
#else
      modifiers |= blink::WebInputEvent::kControlKey;
#endif
    }
  }

  return std::make_pair(modifiers, key);
}

class SimpleMouseEvent : public blink::WebInputEvent {
 public:
  std::unique_ptr<WebInputEvent> Clone() const override {
    return std::make_unique<SimpleMouseEvent>(*this);
  }

  bool CanCoalesce(const blink::WebInputEvent& event) const override {
    return true;
  };

  // Merge the current event with attributes from |event|.
  void Coalesce(const WebInputEvent& event) override{};

  int click_count_;
  gfx::PointF point_;
};

std::unique_ptr<blink::WebInputEvent> CreateMouseEvent(
    int type,
    int buttons,
    float x,
    float y,
    const std::string& modifiers_str) {
  auto res = std::make_unique<SimpleMouseEvent>();
  auto [modifiers, _] = ExtractModifiers(modifiers_str);
  modifiers |= type;
  modifiers |= buttons;
  res->point_ = {x, y};
  res->SetType((blink::WebInputEvent::Type)type);
	res->click_count_ = 1;
  return res;
}

int GetClickCount(const blink::WebInputEvent& event) {
  auto& mouse_event = static_cast<const SimpleMouseEvent&>(event);

  return mouse_event.click_count_;
}

gfx::PointF GetMousePosition(const blink::WebInputEvent& event) {
  const auto& mouse_event = static_cast<const SimpleMouseEvent&>(event);
  return mouse_event.point_;
}

// Adapted from: //content/web_test/renderer/event_sender.cc
std::unique_ptr<blink::WebKeyboardEvent> TranslateKeyEvent(
    int type,
    const std::string& keys) {
  auto [modifiers, key] = ExtractModifiers(keys);
  std::unique_ptr<blink::WebKeyboardEvent> result =
      std::make_unique<blink::WebKeyboardEvent>();

  absl::optional<char16_t> shifted_char;
  ui::KeyboardCode keyCode = electron::KeyboardCodeFromStr(key, &shifted_char);
  result->windows_key_code = keyCode;
  if (shifted_char)
    modifiers |= blink::WebInputEvent::Modifiers::kShiftKey;

  ui::DomCode domCode = ui::UsLayoutKeyboardCodeToDomCode(keyCode);
  result->dom_code = static_cast<int>(domCode);

  ui::DomKey domKey;
  ui::KeyboardCode dummy_code;
  int web_event_flags = 0;
  if (modifiers & blink::WebInputEvent::Modifiers::kShiftKey)
    web_event_flags |= ui::EF_SHIFT_DOWN;
  if (ui::DomCodeToUsLayoutDomKey(domCode, web_event_flags, &domKey,
                                  &dummy_code))
    result->dom_key = static_cast<int>(domKey);

  size_t text_length_cap = blink::WebKeyboardEvent::kTextLengthCap;
  std::u16string text16 = base::UTF8ToUTF16(key);
  std::fill_n(result->text, text_length_cap, 0);
  std::fill_n(result->unmodified_text, text_length_cap, 0);
  if (modifiers & blink::WebInputEvent::Modifiers::kControlKey) {
		// key events with Control are passed as control characters starting with A/a as 1
		if (text16[0] >= 'a' && text16[0] <= 'z') {
			result->text[0] = text16[0] - 'a' + 1;
		}
		if (text16[0] >= 'A' && text16[0] <= 'Z') {
			result->text[0] = text16[0] - 'A' + 1;
		}
  } else {
    for (size_t i = 0; i < std::min(text_length_cap - 1, text16.size()); ++i) {
      result->text[i] = text16[i];
      result->unmodified_text[i] = text16[i];
    }
  }
  result->SetModifiers(modifiers);
  result->SetType((blink::WebInputEvent::Type)type);

  return result;
}
}  // namespace electron::office::simulated_input
