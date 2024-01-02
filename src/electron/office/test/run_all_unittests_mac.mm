// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

namespace mac_quirks {
void main(int, char**) {
  // this is necessary for test runners to handle out-of-thread communication correctly with LOK
  [NSApplication sharedApplication];
}
}
