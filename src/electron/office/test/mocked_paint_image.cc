// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// This exists to support lok_tilebuffer

#include "cc/paint/paint_image_builder.h"

namespace cc {

// Mock PaintImage class

PaintImage::PaintImage() = default;
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;

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

}  // namespace cc
