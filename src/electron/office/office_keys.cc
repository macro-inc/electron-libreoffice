// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <cstdint>
#include <unordered_map>

#include "office/office_keys.h"

using L = electron::office::LOKKeyCodes;
using namespace electron::office::DomCode;

namespace electron::office {
static inline const std::unordered_map<DomCode::K, LOKKeyCodes> directMap_{
    {K::ARROW_DOWN, L::DOWN},
    {K::ARROW_UP, L::UP},
    {K::ARROW_LEFT, L::LEFT},
    {K::ARROW_RIGHT, L::RIGHT},
    {K::HOME, L::HOME},
    {K::END, L::END},
    {K::PAGE_UP, L::PAGEUP},
    {K::PAGE_DOWN, L::PAGEDOWN},

    {K::ENTER, L::RETURN},
    {K::ESCAPE, L::ESCAPE},
    {K::TAB, L::TAB},
    {K::BACKSPACE, L::BACKSPACE},
    {K::NUMPAD_BACKSPACE, L::BACKSPACE},
    {K::SPACE, L::SPACE},
    {K::INSERT, L::INSERT},
    {K::DEL, L::DEL},
    {K::NUMPAD_ADD, L::ADD},
    {K::NUMPAD_SUBTRACT, L::SUBTRACT},
    {K::NUMPAD_MULTIPLY, L::MULTIPLY},
    {K::NUMPAD_DIVIDE, L::DIVIDE},
    {K::NUMPAD_DECIMAL, L::POINT},
    {K::NUMPAD_COMMA, L::COMMA},

    {K::OPEN, L::OPEN},
    {K::CUT, L::CUT},
    {K::COPY, L::COPY},
    {K::PASTE, L::PASTE},
    {K::UNDO, L::UNDO},
    {K::REDO, L::REPEAT},
    {K::FIND, L::FIND},
    {K::PROPS, L::PROPERTIES},
    {K::CONTEXT_MENU, L::CONTEXTMENU},
    {K::HELP, L::HELP},
    {K::NUM_LOCK, L::NUMLOCK},
    {K::CAPS_LOCK, L::CAPSLOCK},
    {K::SCROLL_LOCK, L::SCROLLLOCK},
};

inline int unmappedKey(int dom_code, int lok_modifiers) {
  if (dom_code >= K::F1 && dom_code <= K::F12) {
    return (dom_code - K::F1) + L::F1;
  }
  if (dom_code >= K::F13 && dom_code <= K::F24) {
    return (dom_code - K::F13) + L::F13;
  }
  // cases after this only apply if there is a modifier key applied
  if (!lok_modifiers) {
    return 0;
  }

  if (dom_code >= K::US_A && dom_code <= K::US_Z) {
    return (dom_code - K::US_A) + L::A;
  }
  if (dom_code >= K::DIGIT1 && dom_code <= K::DIGIT9) {
    return (dom_code - K::DIGIT1) + L::NUM1;
  }
  if (dom_code >= K::NUMPAD1 && dom_code <= K::NUMPAD9) {
    return (dom_code - K::NUMPAD1) + L::NUM1;
  }

  // the DomCode order is 1-9,0 but the LibreOffice order is 0-9, this
  // fixes the case for 0 keys
  if (dom_code == K::DIGIT0 || dom_code == K::NUMPAD0) {
    return L::NUM0;
  }

  return 0;
}

int DOMKeyCodeToLOKKeyCode(int dom_code, int modifiers) {
  int result = 0;
  int lok_modifiers = EventModifiersToLOKModifiers(modifiers);

  auto match = directMap_.find((DomCode::K)dom_code);
  // not found
  if (match == directMap_.end()) {
    result = unmappedKey(dom_code, lok_modifiers);
  } else {
    result = match->second;
  }

  // apply the modifiers to the keycode
  result |= lok_modifiers;

  return result;
}

}  // namespace electron::office
