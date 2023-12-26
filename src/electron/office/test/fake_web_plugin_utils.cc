// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/web_plugin_utils.h"
#include "fake_web_plugin_container.h"
#include "fake_ui_clipboard.h"
#include "ui/gfx/geometry/point_f.h"
#include "simulated_input.h"

namespace electron::office {

namespace container {
bool Initialize(blink::WebPluginContainer* container) {
  return true;
}

float DeviceScale(blink::WebPluginContainer* container) {
	return container->device_scale_factor_;
}

std::string CSSCursor(blink::WebPluginContainer* container) {
	return container->css_cursor_;
}

void Invalidate(blink::WebPluginContainer* container) {
	container->invalidate_count_++;
	if (container->invalidate_promise_) {
		container->invalidate_promise_->Resolve();
	}
}
}  // namespace container

namespace input {
gfx::PointF GetRelativeMousePosition(const blink::WebInputEvent& event,
                                gfx::Vector2dF /* delta */) {
	return simulated_input::GetMousePosition(event);
}

int GetClickCount(const blink::WebInputEvent& event) {
	return simulated_input::GetClickCount(event);
}
}  // namespace input

namespace clipboard {
ui::Clipboard* GetCurrent() {
	static thread_local ui::Clipboard clip;
  return &clip;
}

const std::vector<std::u16string> GetAvailableTypes(ui::Clipboard* clipboard) {
	return clipboard->available_types_;
}

std::string ReadTextUtf8(ui::Clipboard* clipboard) {
  return "unreal clipboard";
}

std::vector<uint8_t> ReadPng(ui::Clipboard* clipboard) {
	// smallest valid, transparent PNG
  static std::vector<uint8_t> image {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 
    0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 
    0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 
    0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 
    0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 
    0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49, 
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
	
  return image;
}

}  // namespace clipboard
}  // namespace electron::office
