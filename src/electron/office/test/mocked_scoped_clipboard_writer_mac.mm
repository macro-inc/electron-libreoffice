// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// This exists because //ui/base/clipboard:clipboard_test_support includes the
// entirety of //ui/base and //ui/gfx just to support PNGs

// TODO: actually mock these so that the clipboard can be checked (may not be
// necessary if we switch to just exposing the system clipbaord in LOK directly)

#include "ui/base/clipboard/scoped_clipboard_writer.h"

#import <Cocoa/Cocoa.h>

namespace ui {

ClipboardFormatType::ClipboardFormatType(NSString* native_format)
    : data_([native_format retain]) {}

const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypePNG);
  return *type;
}

}  // namespace ui
