// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <atomic>

namespace blink {
class WebPluginContainer {
public:
	float device_scale_factor_;
	std::string css_cursor_;
	std::atomic<int> invalidate_count_;
};
}
