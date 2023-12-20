// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// These are simple, bare-minimum shims to get OfficeWebPlugin working

#include <string>
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"

namespace blink {
class WebString {
 public:
  WebString();
  ~WebString();
  WebString(WebString const&);
  WebString(WebString&&);
  std::string Ascii() const;
  static WebString FromUTF8(const char* data, size_t length);
};
}  // namespace blink

namespace blink {
WebString::WebString() = default;
WebString::~WebString() = default;
WebString::WebString(WebString const&) = default;
WebString::WebString(WebString&&) = default;
std::string WebString::Ascii() const {
  return "";
}
WebString WebString::FromUTF8(const char* data, size_t length) {
  return {};
}

class WebNode {
 public:
  WebNode();
  ~WebNode();
};

WebNode::WebNode() = default;
WebNode::~WebNode() = default;

std::unique_ptr<WebInputEvent> WebKeyboardEvent::Clone() const {
  return nullptr;
}
bool WebKeyboardEvent::CanCoalesce(const WebInputEvent& event) const {
  return true;
}
void WebKeyboardEvent::Coalesce(const WebInputEvent& event) {}

const WebInputEvent& WebCoalescedInputEvent::Event() const {
  return *event_.get();
}

}  // namespace blink
