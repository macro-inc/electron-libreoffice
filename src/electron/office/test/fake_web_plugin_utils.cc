// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "fake_web_plugin_container.h"
#include "office/web_plugin_utils.h"
#include "simulated_input.h"
#include "ui/gfx/geometry/point_f.h"

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
  if (container->invalidated) {
    std::move(container->invalidated).Run();
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
