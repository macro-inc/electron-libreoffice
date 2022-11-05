// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "electron/office/lok_tilebuffer.h"
#include "base/check.h"
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

// Enable to display debug painting
// #define TILEBUFFER_DEBUG_PAINT

namespace electron::office {

TileBuffer::TileBuffer(lok::Document* document, float scale, int part)
    : document_(document), scale_(scale), part_(part) {
  DCHECK(document_);
  document_->getDocumentSize(&doc_width_twips_, &doc_height_twips_);

  doc_width_scaled_px_ = lok_callback::TwipToPixel(doc_width_twips_, scale_);
  doc_height_scaled_px_ = lok_callback::TwipToPixel(doc_height_twips_, scale_);

  tile_size_scaled_px_ = kTileSizePx * scale_;

  LibreOfficeKitTileMode tile_mode =
      static_cast<LibreOfficeKitTileMode>(document_->getTileMode());

  image_info_ =
      SkImageInfo::Make(tile_size_scaled_px_, tile_size_scaled_px_,
                        tile_mode == LOK_TILEMODE_BGRA ? kBGRA_8888_SkColorType
                                                       : kRGBA_8888_SkColorType,
                        kPremul_SkAlphaType);

  pool_buffer_stride_ = image_info_.computeMinByteSize();
  pool_size_ = kPoolAllocatedSize / pool_buffer_stride_;

  columns_ = std::ceil(static_cast<double>(doc_width_scaled_px_) /
                       tile_size_scaled_px_);
  rows_ = std::ceil(static_cast<double>(doc_height_scaled_px_) /
                    tile_size_scaled_px_);

  pool_buffer_ = std::make_unique<uint8_t[]>(kPoolAllocatedSize);
  pool_bitmaps_ = std::make_unique<SkBitmap[]>(pool_size_ - 1);
  pool_index_to_tile_index_ = std::make_unique<int[]>(pool_size_ - 1);
  for (size_t index = 0; index < pool_size_ - 1; index++) {
    // set the bitmap allocation to the installed pixels
    pool_bitmaps_[index].installPixels(image_info_, GetPoolBuffer(index),
                                       image_info_.minRowBytes());
    // share memory, don't copy
    pool_bitmaps_[index].setImmutable();

    // invalidate the tiles
    pool_index_to_tile_index_[index] = -1;
  }
}

TileBuffer::~TileBuffer() = default;

// transfer, because there shouldn't logically be more than one instance
TileBuffer::TileBuffer(const TileBuffer& o) = default;

void TileBuffer::PaintTile(uint8_t* buffer, int column, int row) {
  DCHECK(document_);
  int index = CoordToIndex(column, row);

  document_->paintPartTile(buffer, part_, tile_size_scaled_px_,
                           tile_size_scaled_px_, kTileSizeTwips * column,
                           kTileSizeTwips * row, kTileSizeTwips,
                           kTileSizeTwips);

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
               doc_height_twips_, kTileSizePx * lok_callback::kTwipPerPx);
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
      int tile_index = CoordToIndex(column, row);
      size_t pool_index;
      if (!TileToPoolIndex(tile_index, &pool_index)) {
        valid_tile_.erase(tile_index);
        InvalidatePoolTile(current_index_);
        pool_index = current_index_;
        tile_index_to_pool_index_[tile_index] = pool_index;
        pool_index_to_tile_index_[pool_index] = tile_index;
        current_index_ = NextPoolIndex();
      }

      if (valid_tile_.count(tile_index) == 0) {
        PaintTile(GetPoolBuffer(pool_index), column, row);
      }

      canvas.drawImage(pool_bitmaps_[pool_index].asImage(),
                       tile_size_scaled_px_ * column,
                       tile_size_scaled_px_ * row);
#ifdef TILEBUFFER_DEBUG_PAINT
      SkPaint debugPaint;
      debugPaint.setARGB(255, 255, 64, 0);
      debugPaint.setStroke(true);
      debugPaint.setStrokeWidth(1);
      SkRect rect{(float)tile_size_scaled_px_ * column,
                  (float)tile_size_scaled_px_ * row,
                  (float)tile_size_scaled_px_ * (column + 1),
                  (float)tile_size_scaled_px_ * (row + 1)

      };
      canvas.drawRect(rect, debugPaint);
#endif
    }
  }
}

}  // namespace electron::office
