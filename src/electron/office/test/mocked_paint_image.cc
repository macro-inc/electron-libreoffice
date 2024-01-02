// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// This exists to support lok_tilebuffer

#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_flags.h"

// these are necessary to compile PaintImage, since they heavily use code in the header itself
#include "cc/paint/paint_shader.h"
#include "cc/paint/paint_filter.h"

namespace cc {

const PaintImage::ContentId PaintImage::kInvalidContentId = -1;

// Mock PaintImage class

PaintImage::PaintImage() = default;
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;
PaintImage& PaintImage::operator=(const PaintImage& other) = default;
PaintImage& PaintImage::operator=(PaintImage&& other) = default;

int PaintImage::GetNextId() {
  static int next_id = 1;
  return next_id++;
}

int PaintImage::GetNextContentId() {
  static int next_content_id = 1;
  return next_content_id++;
}

PaintImageBuilder::PaintImageBuilder(PaintImageBuilder&& other) = default;
PaintImageBuilder::~PaintImageBuilder() = default;

// Mock PaintImageBuilder class
PaintImageBuilder::PaintImageBuilder() = default;
PaintImageBuilder PaintImageBuilder::WithDefault() {
  return PaintImageBuilder();
}

PaintImage PaintImageBuilder::TakePaintImage() {
  return PaintImage();
}

PaintFlags::PaintFlags() = default;
PaintFlags::PaintFlags(const PaintFlags& flags) = default;
PaintFlags::PaintFlags(PaintFlags&& other) = default;
PaintFlags::~PaintFlags() = default;
PaintFlags& PaintFlags::operator=(const PaintFlags& other) = default;
PaintFlags& PaintFlags::operator=(PaintFlags&& other) = default;

}  // namespace cc
