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

#if BUILDFLAG(IS_LINUX)
ClipboardFormatType::ClipboardFormatType(const std::string& native_format)
    : data_(native_format) {}
#endif

ClipboardFormatType::~ClipboardFormatType() = default;

#if BUILDFLAG(IS_LINUX)
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type("image/png");
  return *type;
}
#elif BUILDFLAG(IS_WIN)
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"PNG"));
  return *format;
}
#endif

DataTransferEndpoint::~DataTransferEndpoint() = default;

ScopedClipboardWriter::ScopedClipboardWriter(
    ClipboardBuffer buffer,
    std::unique_ptr<DataTransferEndpoint> data_src)
    : buffer_(buffer), data_src_(std::move(data_src)) {}

void ScopedClipboardWriter::WriteText(const std::u16string& text) {}

void ScopedClipboardWriter::WriteImage(const SkBitmap& bitmap) {}

ScopedClipboardWriter::~ScopedClipboardWriter() = default;

}  // namespace ui
