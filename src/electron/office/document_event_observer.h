// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_DOCUMENT_EVENT_OBSERVER_H_
#define OFFICE_DOCUMENT_EVENT_OBSERVER_H_

#include "base/observer_list_types.h"

namespace electron::office {
class DocumentEventObserver : public base::CheckedObserver {
 public:
  virtual void DocumentCallback(int type, std::string payload) = 0;
};
}  // namespace electron::office

#endif // OFFICE_OFFICE_EVENT_OBSERVER_H_
