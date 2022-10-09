// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_KEYS_H_
#define OFFICE_OFFICE_KEYS_H_

#include <cstdint>
#include "third_party/blink/public/common/input/web_input_event.h"
namespace electron::office {

using Modifiers = blink::WebInputEvent::Modifiers;

// based on include/vcl/keycodes.hxx
enum LOKModifiers : unsigned short {
  Shift = 0x1000,
  Mod1 = 0x2000,
  Mod2 = 0x4000,
  Mod3 = 0x8000,
  Mask = 0xF000,
};
// based on com/sun/star/awt/Key.idl and include/vcl/keycodes.hxx
enum LOKKeyCodes : unsigned short {
  NUM0 = 256,
  NUM1,
  NUM2,
  NUM3,
  NUM4,
  NUM5,
  NUM6,
  NUM7,
  NUM8,
  NUM9,
  A = 512,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  F1 = 768,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  F16,
  F17,
  F18,
  F19,
  F20,
  F21,
  F22,
  F23,
  F24,
  F25,
  F26,

  DOWN = 1024,
  UP,
  LEFT,
  RIGHT,
  HOME,
  END,
  PAGEUP,
  PAGEDOWN,

  RETURN = 1280,
  ESCAPE,
  TAB,
  BACKSPACE,
  SPACE,
  INSERT,
  DELETE,
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  POINT,
  COMMA,
  LESS,
  GREATER,
  EQUAL,
  OPEN,
  CUT,
  COPY,
  PASTE,
  UNDO,
  REPEAT,
  FIND,
  PROPERTIES,
  FRONT,
  CONTEXTMENU,
  MENU,
  HELP,
  HANGUL_HANJA,
  DECIMAL,
  TILDE,
  QUOTELEFT,
  BRACKETLEFT,
  BRACKETRIGHT,
  SEMICOLON,
  QUOTERIGHT,
  CAPSLOCK,
  NUMLOCK,
  SCROLLLOCK,
};
int DOMKeyCodeToLOKKeyCode(int dom_code, int modifiers);

inline int EventModifiersToLOKModifiers(int modifiers) {
  int result = 0;
  if (modifiers & Modifiers::kShiftKey)
    result |= LOKModifiers::Shift;
  if (modifiers & Modifiers::kControlKey)
    result |= LOKModifiers::Mod1;
  if (modifiers & Modifiers::kAltKey)
    result |= LOKModifiers::Mod2;
  if (modifiers & Modifiers::kMetaKey)
    result |= LOKModifiers::Mod3;

  return result;
}

namespace DomCode {
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) id = usb
#define DOM_CODE_DECLARATION enum K : int32_t
#include "ui/events/keycodes/dom/dom_code_data.inc"
#undef DOM_CODE
#undef DOM_CODE_DECLARATION
}  // namespace DomCode

}  // namespace electron::office
#endif  // OFFICE_OFFICE_KEYS_H_
