// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

/**
 * This is intended to abstract away the blink::, content::, ui:: specific
 * functionality of the web plugin. Basically anything that interfaces with
 * Chromium internals not specific to implementing OfficeWebPlugin itself. This
 * greatly simplifies testing and keeps the code cleaner overall.
 */

#include <string>
namespace blink {
class WebPluginContainer;
class WebInputEvent;
}  // namespace blink

namespace gfx {
class Vector2dF;
class PointF;
}  // namespace gfx

namespace ui {
class Clipboard;
}

namespace electron::office {
namespace container {
float DeviceScale(blink::WebPluginContainer* container);
bool Initialize(blink::WebPluginContainer* container);
std::string CSSCursor(blink::WebPluginContainer* container);
void Invalidate(blink::WebPluginContainer* container);
}  // namespace container

namespace input {
gfx::PointF GetRelativeMousePosition(const blink::WebInputEvent& event,
                                gfx::Vector2dF delta);
int GetClickCount(const blink::WebInputEvent& event);
}
}  // namespace electron::office
