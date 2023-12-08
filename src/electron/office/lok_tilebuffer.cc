// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "electron/office/lok_tilebuffer.h"
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/skia_paint_canvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTextBlob.h"
#include "office/cancellation_flag.h"
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

// Uncomment to display debug painting
// #define TILEBUFFER_DEBUG_PAINT

namespace electron::office {
TileBuffer::TileBuffer()
    : base::RefCountedDeleteOnSequence<TileBuffer>(
          base::SequencedTaskRunnerHandle::Get()),
      valid_tile_(0),
      active_context_hash_(0) {
  pool_buffer_ =
      std::shared_ptr<uint8_t[]>(static_cast<uint8_t*>(base::AlignedAlloc(
                                     kPoolAllocatedSize, kPoolAligned)),
                                 base::AlignedFreeDeleter{});

  std::fill_n(pool_index_to_tile_index_, kPoolSize, kInvalidTileIndex);
}

Snapshot::Snapshot(std::vector<cc::PaintImage> tiles_,
                   float scale_,
                   int column_start_,
                   int column_end_,
                   int row_start_,
                   int row_end_,
                   int scroll_y_position)
    : tiles(std::move(tiles_)),
      scale(scale_),
      column_start(column_start_),
      column_end(column_end_),
      row_start(row_start_),
      row_end(row_end_),
      scroll_y_position(scroll_y_position) {}

Snapshot::Snapshot() = default;
Snapshot::~Snapshot() = default;
Snapshot::Snapshot(const Snapshot& other) = default;
Snapshot& Snapshot::operator=(const Snapshot& other) = default;
Snapshot& Snapshot::operator=(Snapshot&& other) noexcept = default;
Snapshot::Snapshot(Snapshot&& other) noexcept = default;

TileBuffer::~TileBuffer() = default;

void TileBuffer::Resize(long width_twips, long height_twips, float scale) {
  doc_width_twips_ = width_twips;
  doc_height_twips_ = height_twips;
  scale_ = scale;

  doc_width_scaled_px_ = lok_callback::TwipToPixel(doc_width_twips_, scale_);
  doc_height_scaled_px_ = lok_callback::TwipToPixel(doc_height_twips_, scale_);

  columns_ = std::ceil(static_cast<double>(doc_width_scaled_px_) / kTileSizePx);
  rows_ = std::ceil(static_cast<double>(doc_height_scaled_px_) / kTileSizePx);

  valid_tile_ = AtomicBitset(columns_ * rows_ + 1);
  std::fill_n(pool_index_to_tile_index_, kPoolSize, kInvalidTileIndex);
}

void TileBuffer::Resize(long width_twips, long height_twips) {
  if (doc_width_twips_ != width_twips || doc_height_twips_ != height_twips)
    Resize(width_twips, height_twips, scale_);
}

void TileBuffer::SetActiveContext(std::size_t active_context_hash) {
  active_context_hash_ = active_context_hash;
}

void TileBuffer::ResetScale(float scale) {
  if (std::abs(scale - scale_) > 0.001) {
    Resize(doc_width_twips_, doc_height_twips_, scale);
    SetActiveContext(0);
  }
}

bool TileBuffer::PaintTile(CancelFlagPtr cancel_flag,
                           DocumentHolderWithView document,
                           unsigned int tile_index,
                           std::size_t context_hash) {
  static const SkImageInfo image_info_ = SkImageInfo::Make(
      kTileSizePx, kTileSizePx, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  size_t pool_index;
  const unsigned int max = columns_ * rows_ - 1;
  if (const std::size_t ah = active_context_hash_; ah != context_hash) {
    valid_tile_.Clear();
    return false;
  }

  if (tile_index > max) {
    // TODO: proper fix, this probably occurs after a zoom
    LOG(ERROR) << "invalid tile index: " << tile_index << ", exceeds max "
               << max << " ach: " << std::hex << active_context_hash_
               << " ch: " << context_hash;
    LOG(ERROR) << "BAD CONTEXT CLEAR != " << std::hex << context_hash;
    valid_tile_.Clear();
    return false;
  }

  if (!TileToPoolIndex(tile_index, &pool_index)) {
    InvalidatePoolTile(pool_index);
    pool_index_to_tile_index_[pool_index] = tile_index;
  }

  if (!CancelFlag::IsCancelled(cancel_flag) &&
      tile_index < valid_tile_.Size() && !valid_tile_[tile_index]) {
    std::pair<int, int> coord = IndexToCoord(tile_index);
    int column = coord.first;
    int row = coord.second;
    uint8_t* buffer = GetPoolBuffer(pool_index);
    std::fill_n(reinterpret_cast<uint32_t*>(buffer),
                kBufferStride / sizeof(uint32_t), SK_ColorTRANSPARENT);
    document->paintTile(buffer, kTileSizePx, kTileSizePx,
                        lok_callback::PixelToTwip(kTileSizePx * column, scale_),
                        lok_callback::PixelToTwip(kTileSizePx * row, scale_),
                        lok_callback::PixelToTwip(kTileSizePx, scale_),
                        lok_callback::PixelToTwip(kTileSizePx, scale_));

    if (const std::size_t ah = active_context_hash_; ah != context_hash) {
      valid_tile_.Clear();
      return false;
    }
    sk_sp<SkImage> image = SkImage::MakeRasterData(
        image_info_,
        SkData::MakeWithCopy(GetPoolBuffer(pool_index), kBufferStride),
        kTileSizePx * kBytesPerPx);
    pool_paint_images_[pool_index] =
        cc::PaintImageBuilder::WithDefault()
            .set_id(cc::PaintImage::GetNextId())
            .set_image(image, cc::PaintImage::GetNextContentId())
            .TakePaintImage();

    // because valid_tile is critical to render, check after rasterization
    if (const std::size_t ah = active_context_hash_; ah != context_hash) {
      valid_tile_.Clear();
      return false;
    }

    valid_tile_.Set(tile_index);
    return true;
  }

  return tile_index < valid_tile_.Size() && valid_tile_[tile_index];
}

void TileBuffer::InvalidateTile(unsigned int column, unsigned int row) {
  InvalidateTile(CoordToIndex(column, row));
}

void TileBuffer::InvalidateTile(size_t index) {
  valid_tile_.Reset(index);
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

TileRange TileBuffer::InvalidateTilesInRect(const gfx::RectF& rect,
                                            bool dry_run) {
  auto tile_rect =
      TileRect(rect, doc_width_scaled_px_, doc_height_scaled_px_, kTileSizePx);
  DCHECK(tile_rect.x() >= 0);
  DCHECK(tile_rect.y() >= 0);
  DCHECK(tile_rect.width() >= 0);
  DCHECK(tile_rect.height() >= 0);
  DCHECK((unsigned int)tile_rect.right() <= columns_);
  DCHECK((unsigned int)tile_rect.bottom() <= rows_);

  unsigned int index_start = CoordToIndex(tile_rect.x(), tile_rect.y());
  unsigned int index_end =
      CoordToIndex(std::min((unsigned int)tile_rect.right(), columns_ - 1),
                   std::min((unsigned int)tile_rect.bottom(), rows_ - 1));
  if (!dry_run)
    valid_tile_.ResetRange(index_start, index_end);
  return {index_start, index_end};
}

std::vector<TileRange> TileBuffer::InvalidRangesRemaining(
    std::vector<TileRange> tile_ranges) {
  std::vector<TileRange> result;

  size_t pool_index;
  for (auto& it : tile_ranges) {
    for (unsigned int i = it.index_start;
         i <= it.index_end && i < valid_tile_.Size(); i++) {
      if (!valid_tile_[i] || !TileToPoolIndex(i, &pool_index)) {
        if (!result.empty() && result.back().index_end == i - 1) {
          result.back().index_end = i;
        } else {
          result.emplace_back(i, i);
        }
      }
    }
  }

  return SimplifyRanges(result);
}

TileBuffer::RowLimit TileBuffer::LimitRange(int y_pos,
                                            unsigned int view_height) {
  unsigned int start_row = y_pos < 0 ? 0 :
      std::floor((double)y_pos / (double)TileBuffer::kTileSizePx);
  unsigned int end_row = start_row + std::ceil((double)view_height /
                                               (double)TileBuffer::kTileSizePx);
  return {start_row, std::max(start_row, end_row)};
}

TileRange TileBuffer::LimitIndex(int y_pos, unsigned int view_height) {
  auto row_limit = LimitRange(y_pos, view_height);
  unsigned int start_limit = CoordToIndex(0, row_limit.start);
  unsigned int end_limit = CoordToIndex(columns_ - 1, row_limit.end);

  return {start_limit, end_limit};
}

std::vector<TileRange> TileBuffer::ClipRanges(std::vector<TileRange> ranges,
                                              TileRange range_limit) {
  std::vector<TileRange> clipped_ranges;
  for (const TileRange& range : ranges) {
    if (range.index_end < range_limit.index_start ||
        range.index_start > range_limit.index_end) {
      // The range is completely outside the limit range, ignore it.
      continue;
    }

    clipped_ranges.emplace_back(
        std::max(range.index_start, range_limit.index_start),
        std::min(range.index_end, range_limit.index_end));
  }
  return clipped_ranges;
}

TileRange TileBuffer::NextScrollTileRange(int next_y_pos,
                                          unsigned int view_height) {
  next_y_pos = std::max(0, next_y_pos - (int)view_height);
  auto row_limit = LimitRange(next_y_pos, view_height * 3);
  unsigned int start_row = row_limit.start > 0 ? row_limit.start : 0;
  unsigned int end_row = row_limit.end;

  unsigned int index_start = std::min(start_row, rows_ - 1) * columns_;
  unsigned int index_end =
      std::min(end_row, rows_ - 1) * columns_ + columns_ - 1;
  unsigned int limit = columns_ * rows_ - 1;

  return {std::min(index_start, limit), std::min(index_end, limit)};
}

TileRange TileBuffer::InvalidateTilesInTwipRect(const gfx::Rect& rect_twips) {
  auto tile_rect = TileRect(std::move(gfx::RectF(rect_twips)), doc_width_twips_,
                            doc_height_twips_,
                            lok_callback::PixelToTwip(kTileSizePx, scale_));
  DCHECK(tile_rect.x() >= 0);
  DCHECK(tile_rect.y() >= 0);
  DCHECK(tile_rect.width() >= 0);
  DCHECK(tile_rect.height() >= 0);
  DCHECK((unsigned int)tile_rect.right() <= columns_);
  DCHECK((unsigned int)tile_rect.bottom() <= rows_);

  unsigned int index_start = CoordToIndex(tile_rect.x(), tile_rect.y());
  unsigned int index_end =
      CoordToIndex(std::min((unsigned int)tile_rect.right(), columns_ - 1),
                   std::min((unsigned int)tile_rect.bottom(), rows_ - 1));

  valid_tile_.ResetRange(index_start, index_end);
  return {index_start, index_end};
}

void TileBuffer::InvalidateAllTiles() {
  SetActiveContext(0);
  valid_tile_.Clear();
}

void TileBuffer::SetYPosition(float y) {
  y_pos_ = y;
}

std::vector<TileRange> TileBuffer::PaintToCanvas(CancelFlagPtr cancel_flag,
                                                 cc::PaintCanvas* canvas,
                                                 const Snapshot& snapshot,
                                                 const gfx::Rect& rect,
                                                 float total_scale,
                                                 bool scale_pending,
                                                 bool scrolling) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  canvas->translate(0, -y_pos_);

  auto offset_rect = gfx::RectF(rect);
  offset_rect.Offset(0, y_pos_);
  gfx::Rect tile_rect = TileRect(offset_rect, doc_width_scaled_px_,
                                 doc_height_scaled_px_, kTileSizePx);

  DCHECK(tile_rect.x() >= 0);
  DCHECK(tile_rect.y() >= 0);
  DCHECK(tile_rect.width() >= 0);
  DCHECK(tile_rect.height() >= 0);
  DCHECK((unsigned int)tile_rect.right() <= columns_);
  DCHECK((unsigned int)tile_rect.bottom() <= rows_);
  std::vector<TileRange> missing_ranges;

  unsigned int row_start = (unsigned int)tile_rect.y();
  unsigned int row_end = (unsigned int)tile_rect.bottom();
  unsigned int column_start = (unsigned int)tile_rect.x();
  unsigned int column_end = (unsigned int)tile_rect.right();

  int last_good_row = -1;
  // dry run to check for missing tiles
  for (unsigned int row = row_start; row < row_end; ++row) {
    for (unsigned int column = column_start; column < column_end; ++column) {
      unsigned int tile_index = CoordToIndex(column, row);
      size_t pool_index;

      if (!TileToPoolIndex(tile_index, &pool_index)) {
        if (missing_ranges.empty() ||
            missing_ranges.back().index_end + 1 != tile_index) {
          missing_ranges.emplace_back(tile_index, tile_index);
        } else {
          missing_ranges.back().index_end = tile_index;
        }
        // tracking the last good row prevents rendering a partial row which can
        // appear glitchy while scrolling
        if (last_good_row == -1)
          last_good_row = std::max((int)row, (int)row_start);
        continue;
      }

#ifdef TILEBUFFER_DEBUG_PAINT
      cc::PaintFlags debugPaint;
      debugPaint.setColor(SK_ColorRED);
      debugPaint.setStrokeWidth(1);
      SkRect debugRect{(float)kTileSizePx * column, (float)kTileSizePx * row,
                       (float)kTileSizePx * (column + 1),
                       (float)kTileSizePx * (row + 1)};

      SkFont font;
      font.setScaleX(0.5);
      font.setSize(12 * scale_);
      canvas->drawTextBlob(
          SkTextBlob::MakeFromString(std::to_string(tile_index).c_str(), font),
          debugRect.left(), debugRect.bottom(), debugPaint);

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

  if (last_good_row != -1) {
    row_end = last_good_row;
  }

  // draw the tiles if none are missing
  if (scrolling || (missing_ranges.empty() && !scale_pending)) {
    for (unsigned int row = row_start; row < row_end; ++row) {
      for (unsigned int column = column_start; column < column_end; ++column) {
        if (CancelFlag::IsCancelled(cancel_flag)) {
          return missing_ranges;
        }

        unsigned int tile_index = CoordToIndex(column, row);
        size_t pool_index;

        if (!TileToPoolIndex(tile_index, &pool_index)) {
          return missing_ranges;
        }
        canvas->drawImage(pool_paint_images_[pool_index], kTileSizePx * column,
                          kTileSizePx * row,
                          SkSamplingOptions(SkFilterMode::kLinear), &flags);
#ifdef TILEBUFFER_DEBUG_PAINT
        cc::PaintFlags debugPaint;
        debugPaint.setColor(SK_ColorBLUE);
        debugPaint.setStrokeWidth(1);
        SkRect debugRect{(float)kTileSizePx * column, (float)kTileSizePx * row,
                         (float)kTileSizePx * (column + 1),
                         (float)kTileSizePx * (row + 1)};

        SkFont font;
        font.setScaleX(0.5);
        font.setSize(12 * scale_);
        canvas->drawTextBlob(SkTextBlob::MakeFromString(
                                 std::to_string(tile_index).c_str(), font),
                             debugRect.left(), debugRect.bottom(), debugPaint);

        debugPaint.setColor(SK_ColorGREEN);
        canvas->drawLine(debugRect.left(), debugRect.top(), debugRect.right(),
                         debugRect.top(), debugPaint);
        canvas->drawLine(debugRect.right(), debugRect.top(), debugRect.right(),
                         debugRect.bottom(), debugPaint);
        canvas->drawLine(debugRect.right(), debugRect.bottom(),
                         debugRect.left(), debugRect.bottom(), debugPaint);
        canvas->drawLine(debugRect.left(), debugRect.top(), debugRect.left(),
                         debugRect.bottom(), debugPaint);
#endif
      }
    }
    return missing_ranges;
  }

  // there are missing tiles, paint the snapshot (unless it isn't set)
  if (snapshot.tiles.empty()) {
    return missing_ranges;
  }

  // this seems redundant, but it's to adjust for scale without an offset that
  // causes jiggling
  canvas->translate(0, y_pos_);
  canvas->scale(total_scale / snapshot.scale);
  canvas->translate(0, -y_pos_);
  std::vector<cc::PaintImage>::const_iterator it = snapshot.tiles.cbegin();
  for (unsigned int row = snapshot.row_start; row < snapshot.row_end; ++row) {
    for (unsigned int column = snapshot.column_start;
         column < snapshot.column_end; ++column) {
      if (CancelFlag::IsCancelled(cancel_flag)) {
        return missing_ranges;
      }
      canvas->drawImage(*it++, kTileSizePx * column, kTileSizePx * row,
                        SkSamplingOptions(SkFilterMode::kLinear), &flags);
#ifdef TILEBUFFER_DEBUG_PAINT
      cc::PaintFlags debugPaint;
      debugPaint.setColor(SK_ColorBLUE);
      debugPaint.setStrokeWidth(1);
      SkRect debugRect{(float)kTileSizePx * column, (float)kTileSizePx * row,
                       (float)kTileSizePx * (column + 1),
                       (float)kTileSizePx * (row + 1)};
      std::string coord;
      coord += std::to_string(column);
      coord += "x";
      coord += std::to_string(row);

      SkFont font;
      font.setScaleX(0.5);
      font.setSize(12 * scale_);
      canvas->drawTextBlob(SkTextBlob::MakeFromString(coord.c_str(), font),
                           debugRect.left(), debugRect.bottom(), debugPaint);

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

  return missing_ranges;
}

bool TileRange::operator==(const TileRange& rhs) const {
  return index_start == rhs.index_start && index_end == rhs.index_end;
}
bool TileRange::operator<(const TileRange& rhs) const {
  return index_start < rhs.index_start;
}

std::vector<TileRange> SimplifyRanges(std::vector<TileRange> tile_ranges_) {
  if (tile_ranges_.size() < 2)
    return tile_ranges_;

  std::vector sorted_copy(tile_ranges_);
  std::vector<TileRange> simplified_copy;

  std::sort(sorted_copy.begin(), sorted_copy.end());

  simplified_copy.emplace_back(sorted_copy[0]);

  for (std::vector<TileRange>::iterator it = sorted_copy.begin() + 1;
       it != sorted_copy.end(); ++it) {
    if (simplified_copy.back().index_end < it->index_start) {
      // there is no overlap, add it
      simplified_copy.emplace_back(it->index_start, it->index_end);
    } else if (simplified_copy.back().index_end < it->index_end) {
      // there is overlap, merge the ends
      simplified_copy.back().index_end = it->index_end;
    }
  }

  return simplified_copy;
}

size_t TileCount(std::vector<TileRange> tile_ranges_) {
  size_t result = 0;
  for (auto& it : tile_ranges_) {
    result += it.index_end - it.index_start + 1;
  }
  return result;
}

bool TileBuffer::IsEmpty() {
  return rows_ == 0 || columns_ == 0;
}

Snapshot TileBuffer::MakeSnapshot(CancelFlagPtr cancel_flag,
                                  const gfx::Rect& rect) {
  std::vector<cc::PaintImage> tiles;

  auto offset_rect = gfx::RectF(rect);
  offset_rect.Offset(0, y_pos_);
  gfx::Rect tile_rect = TileRect(offset_rect, doc_width_scaled_px_,
                                 doc_height_scaled_px_, kTileSizePx);

  DCHECK(tile_rect.x() >= 0);
  DCHECK(tile_rect.y() >= 0);
  DCHECK(tile_rect.width() >= 0);
  DCHECK(tile_rect.height() >= 0);
  DCHECK((unsigned int)tile_rect.right() <= columns_);
  DCHECK((unsigned int)tile_rect.bottom() <= rows_);

  unsigned int row_start = (unsigned int)tile_rect.y();
  unsigned int row_end = (unsigned int)tile_rect.bottom();
  unsigned int column_start = (unsigned int)tile_rect.x();
  unsigned int column_end = (unsigned int)tile_rect.right();

  for (unsigned int row = row_start; row < row_end; ++row) {
    for (unsigned int column = column_start; column < column_end; ++column) {
      unsigned int tile_index = CoordToIndex(column, row);
      size_t pool_index;

      if (!TileToPoolIndex(tile_index, &pool_index)) {
        LOG(ERROR) << "This shouldn't happen";
        return Snapshot();
      }

      tiles.emplace_back(pool_paint_images_[pool_index]);
    }
  }

  return Snapshot(std::move(tiles), scale_, column_start, column_end, row_start,
                  row_end, y_pos_);
}

}  // namespace electron::office
