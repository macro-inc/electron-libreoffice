// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "electron/office/lok_tilebuffer.h"
#include "base/check.h"
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace electron::office {

TileBuffer::TileBuffer(lok::Document* document, float scale, int part)
    : document_(document), scale_(scale), part_(part) {
  DCHECK(document_);
  document_->getDocumentSize(&doc_width_twips_, &doc_height_twips_);

  doc_width_scaled_px_ = lok_callback::TwipToPixel(doc_width_twips_, scale_);
  doc_height_scaled_px_ = lok_callback::TwipToPixel(doc_height_twips_, scale_);

  tile_size_scaled_px_ = kTileSize * scale_;
  tile_size_scaled_twips_ = tile_size_scaled_px_ * lok_callback::kTwipPerPx;

  LibreOfficeKitTileMode tile_mode =
      static_cast<LibreOfficeKitTileMode>(document_->getTileMode());

  image_info_ =
      SkImageInfo::Make(tile_size_scaled_px_, tile_size_scaled_px_,
                        tile_mode == LOK_TILEMODE_BGRA ? kBGRA_8888_SkColorType
                                                       : kRGBA_8888_SkColorType,
                        kPremul_SkAlphaType);

  columns_ = std::ceil(static_cast<double>(doc_width_scaled_px_) /
                       tile_size_scaled_px_);
  rows_ = std::ceil(static_cast<double>(doc_height_scaled_px_) /
                    tile_size_scaled_px_);
}

TileBuffer::~TileBuffer() = default;
TileBuffer::TileBuffer(const TileBuffer&) = default;

void TileBuffer::PaintTile(int column, int row) {
  DCHECK(document_);
  int index = CoordToIndex(column, row);
  DCHECK(tiles_.size() > (size_t)index);

  document_->paintPartTile(
      tiles_.at(index).pixels_, part_, tile_size_scaled_px_, tile_size_scaled_px_,
      tile_size_scaled_twips_ * column, tile_size_scaled_twips_ * row,
      tile_size_scaled_twips_, tile_size_scaled_twips_);

  valid_tile_.emplace(index);
}

void TileBuffer::InvalidateTile(int column, int row) {
  valid_tile_.erase(CoordToIndex(column, row));
}

namespace {
gfx::Rect TileRect(const gfx::RectF& target,
                   float container_width,
                   float container_height,
                   float tile_size) {
  gfx::RectF intersection = gfx::IntersectRects(
      target, std::move(gfx::RectF(container_width, container_height)));
  intersection.Scale(1 / tile_size);
  return gfx::ToEnclosingRect(std::move(intersection));
}
}  // namespace

void TileBuffer::InvalidateTilesInRect(const gfx::RectF& rect) {
  auto tile_rect = TileRect(rect, doc_width_scaled_px_, doc_height_scaled_px_,
                            tile_size_scaled_px_);
  DCHECK(tile_rect.right() <= columns_);
  DCHECK(tile_rect.bottom() <= rows_);

  for (int row = tile_rect.y(); row < tile_rect.bottom(); ++row) {
    for (int column = tile_rect.x(); column < tile_rect.right(); ++column) {
      InvalidateTile(column, row);
    }
  }
}

void TileBuffer::InvalidateTilesInTwipRect(const gfx::Rect& rect_twips) {
  auto tile_rect =
      TileRect(std::move(gfx::RectF(rect_twips)), doc_width_twips_,
               doc_height_twips_, kTileSize * lok_callback::kTwipPerPx);
  DCHECK(tile_rect.right() <= columns_);
  DCHECK(tile_rect.bottom() <= rows_);

  for (int row = tile_rect.y(); row < tile_rect.bottom(); ++row) {
    for (int column = tile_rect.x(); column < tile_rect.right(); ++column) {
      InvalidateTile(column, row);
    }
  }
}

void TileBuffer::InvalidateAllTiles() {
  valid_tile_.clear();
}

void TileBuffer::PaintInvalidTiles(SkCanvas& canvas, const gfx::RectF& rect) {
  auto tile_rect = TileRect(rect, doc_width_scaled_px_, doc_height_scaled_px_,
                            tile_size_scaled_px_);
  DCHECK(tile_rect.right() <= columns_);
  DCHECK(tile_rect.bottom() <= rows_);

  for (int row = tile_rect.y(); row < tile_rect.bottom(); ++row) {
    for (int column = tile_rect.x(); column < tile_rect.right(); ++column) {
      int index = CoordToIndex(column, row);
      auto tile_ = tiles_.try_emplace(index, image_info_).first;
      if (valid_tile_.count(index) == 0) {
        PaintTile(column, row);
      }
      canvas.drawImage(tile_->second.bitmap_.asImage(), tile_size_scaled_px_ * column,
                       tile_size_scaled_px_ * row);
    }
  }
}

TileBuffer::Tile::Tile(const SkImageInfo& info)
    : pixels_(static_cast<uint8_t*>(
          calloc(info.height() * info.width(), info.minRowBytes()))) {
  // set the bitmap allocation to the installed pixels
  bitmap_.installPixels(info, pixels_, info.minRowBytes());
  // share memory, don't copy
  bitmap_.setImmutable();
}

TileBuffer::Tile::~Tile() {
  free(pixels_);
}

}  // namespace electron::office
