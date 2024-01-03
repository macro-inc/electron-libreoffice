// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/web_plugin_utils.h"
#include "fake_web_plugin_container.h"
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
}  // namespace electron::office
