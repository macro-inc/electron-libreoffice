// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_LOAD_OBSERVER_H_
#define OFFICE_LOAD_OBSERVER_H_

#include "base/observer_list_types.h"

namespace lok {
class Office;
}

namespace electron::office {
class OfficeLoadObserver : public base::CheckedObserver {
  // `office` is a plain pointer because the lifetime of lok::Office will be the
  // same as the singleton
 public:
  virtual void OnLoaded(lok::Office* office) = 0;
};
}  // namespace electron::office

#endif
