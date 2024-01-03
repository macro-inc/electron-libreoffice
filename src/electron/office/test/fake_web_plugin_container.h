// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <atomic>
#include "office/promise.h"

namespace blink {
class WebPluginContainer {
public:
	WebPluginContainer();
	~WebPluginContainer();
	float device_scale_factor_;
	std::string css_cursor_ = "default";
	std::atomic<int> invalidate_count_ = 0;
	std::unique_ptr<electron::office::Promise<void>> invalidate_promise_ = nullptr;
};
}
