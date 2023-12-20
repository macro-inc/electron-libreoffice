// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

namespace ui {
class Clipboard {
 public:
  Clipboard();
  ~Clipboard();
  std::vector<std::u16string> available_types_;
};
}  // namespace ui
