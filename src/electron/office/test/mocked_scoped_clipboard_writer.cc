// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// This exists because //ui/base/clipboard:clipboard_test_support includes the
// entirety of //ui/base and //ui/gfx just to support PNGs

// TODO: actually mock these so that the clipboard can be checked (may not be
// necessary if we switch to just exposing the system clipbaord in LOK directly)

#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace ui {

ClipboardFormatType::ClipboardFormatType() = default;
ClipboardFormatType::ClipboardFormatType(const std::string& native_format)
    : data_(native_format) {}

ClipboardFormatType::~ClipboardFormatType() = default;

const char kMimeTypePNG[] = "image/png";

const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypePNG);
  return *type;
}

DataTransferEndpoint::~DataTransferEndpoint() = default;

ScopedClipboardWriter::ScopedClipboardWriter(
    ClipboardBuffer buffer,
    std::unique_ptr<DataTransferEndpoint> data_src)
    : buffer_(buffer), data_src_(std::move(data_src)) {}

void ScopedClipboardWriter::WritePickledData(
    const base::Pickle& pickle,
    const ClipboardFormatType& format) {}

void ScopedClipboardWriter::WriteText(const std::u16string& text) {}

ScopedClipboardWriter::~ScopedClipboardWriter() = default;

}  // namespace ui
