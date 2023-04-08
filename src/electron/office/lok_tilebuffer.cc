// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "electron/office/lok_tilebuffer.h"
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/auto_reset.h"
#include "base/check.h"
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"
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

  tile_size_scaled_px_ = kTileSizePx;

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
  pool_bitmaps_ = std::make_unique<SkBitmap[]>(pool_size_);
  pool_index_to_tile_index_ = std::make_unique<int[]>(pool_size_);
  pool_paint_images_ = std::make_unique<cc::PaintImage[]>(pool_size_);
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

  document_->paintPartTile(
      buffer, part_, tile_size_scaled_px_, tile_size_scaled_px_,
      lok_callback::PixelToTwip(kTileSizePx * column, scale_),
      lok_callback::PixelToTwip(kTileSizePx * row, scale_),
      lok_callback::PixelToTwip(kTileSizePx, scale_),
      lok_callback::PixelToTwip(kTileSizePx, scale_));

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
  auto tile_rect = TileRect(std::move(gfx::RectF(rect_twips)), doc_width_twips_,
                            doc_height_twips_,
                            lok_callback::PixelToTwip(kTileSizePx, scale_));
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

bool TileBuffer::PartiallyPainted() {
  return partially_painted_;
}

void TileBuffer::SetYPosition(float y) {
  if (y != y_pos_) {
    resume_row = -1;
    resume_col = -1;
  }
  y_pos_ = y;
}

void TileBuffer::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  canvas->translate(0, -y_pos_);

  auto offset_rect = gfx::RectF(rect);
  offset_rect.Offset(0, y_pos_);
  gfx::Rect tile_rect = TileRect(offset_rect, doc_width_scaled_px_,
                                 doc_height_scaled_px_, tile_size_scaled_px_);

  DCHECK(tile_rect.right() <= columns_);
  DCHECK(tile_rect.bottom() <= rows_);
  // auto start = std::chrono::steady_clock::now();
  partially_painted_ = true;

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

      if (valid_tile_.find(tile_index) == valid_tile_.end()) {
        PaintTile(GetPoolBuffer(pool_index), column, row);
        pool_paint_images_[pool_index] =
            cc::PaintImage::CreateFromBitmap(pool_bitmaps_[pool_index]);
      }

      canvas->drawImage(
          pool_paint_images_[pool_index], tile_size_scaled_px_ * column,
          tile_size_scaled_px_ * row, SkSamplingOptions(SkFilterMode::kLinear), &flags);

      // if (std::chrono::steady_clock::now() - start > kFrameDeadline) {
      //   // partial paint, mark for resume
      //   break;
      // }

#ifdef TILEBUFFER_DEBUG_PAINT
      cc::PaintFlags debugPaint;
      debugPaint.setColor(SK_ColorRED);
      debugPaint.setStrokeWidth(1);
      SkRect debugRect{(float)tile_size_scaled_px_ * column,
                       (float)tile_size_scaled_px_ * row,
                       (float)tile_size_scaled_px_ * (column + 1),
                       (float)tile_size_scaled_px_ * (row + 1)

      };

      canvas->drawLine(debugRect.left(), debugRect.top(), debugRect.right(),
                       debugRect.top(), debugPaint);
      canvas->drawLine(debugRect.right(), debugRect.top(), debugRect.right(),
                       debugRect.bottom(), debugPaint);
      canvas->drawLine(debugRect.right(), debugRect.bottom(), debugRect.left(),
                       debugRect.bottom(), debugPaint);
      canvas->drawLine(debugRect.left(), debugRect.top(), debugRect.left(),
                       debugRect.bottom(), debugPaint);
#endif
    }
  }

  partially_painted_ = false;
}

}  // namespace electron::office
