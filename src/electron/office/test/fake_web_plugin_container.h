// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include "base/callback.h"

namespace blink {
class WebPluginContainer {
public:
	WebPluginContainer();
	~WebPluginContainer();
	float device_scale_factor_ = 1.0f;
	std::string css_cursor_ = "default";
	base::OnceClosure invalidated;
};
}
