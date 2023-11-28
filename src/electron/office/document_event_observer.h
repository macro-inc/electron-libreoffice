// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include "base/observer_list_types.h"

namespace electron::office {
class DocumentEventObserver : public base::CheckedObserver {
 public:
  virtual void DocumentCallback(int type, std::string payload) = 0;
};
}  // namespace electron::office

