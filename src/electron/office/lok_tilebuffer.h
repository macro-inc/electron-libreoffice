// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_LOK_TILEBUFFER_H_
#define OFFICE_LOK_TILEBUFFER_H_

#include <vector>
#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_canvas.h"
#include "office/atomic_bitset.h"
#include "office/cancellation_flag.h"
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace lok {
class Document;
}

namespace cc {
class PaintCanvas;
class PaintImage;
}  // namespace cc

namespace electron::office {

struct TileRange {
  unsigned int index_start;
  unsigned int index_end;
  bool operator==(const TileRange&) const;
  bool operator<(const TileRange&) const;
  TileRange(unsigned int index_start_, unsigned int index_end_)
      : index_start(index_start_), index_end(index_end_) {
    DCHECK_LE(index_start, index_end);
  }
};

// sorts by lowest index to highest index, merging overlapping indices
std::vector<TileRange> SimplifyRanges(std::vector<TileRange> tile_ranges_);
// the total number of tiles within a vector of tile ranges. assumes the range
// is simplified
size_t TileCount(std::vector<TileRange> tile_ranges_);

struct Snapshot {
  std::vector<cc::PaintImage> tiles;
  float scale = 0.0;
  unsigned int column_start = 0;
  unsigned int column_end = 0;
  unsigned int row_start = 0;
  unsigned int row_end = 0;

  Snapshot(std::vector<cc::PaintImage> tiles_,
           float scale_,
           int column_start_,
           int column_end_,
           int row_start_,
           int row_end_);

  Snapshot();
  ~Snapshot();
};

class TileBuffer {
 public:
  TileBuffer();
  ~TileBuffer();

  static constexpr int kTileSizePx = 256;
  static constexpr int kTileSizeTwips = kTileSizePx * lok_callback::kTwipPerPx;

  // no copy
  TileBuffer(const TileBuffer& other) = delete;
  TileBuffer& operator=(const TileBuffer& other) = delete;

  void InvalidateTile(unsigned int column, unsigned int row);
  void InvalidateTile(size_t pool_index);
  // returns the TileRange of invalidated tiles in the rect
  TileRange InvalidateTilesInRect(const gfx::RectF& rect, bool dry_run = false);
  // returns the TileRange of invalidated tiles in the rect
  TileRange InvalidateTilesInTwipRect(const gfx::Rect& rect_twips);
  // returns the TileRange of tiles for a predicted scroll range
  TileRange NextScrollTileRange(int next_y_pos, unsigned int view_height);
  void InvalidateAllTiles();
  std::vector<TileRange> PaintToCanvas(CancelFlagPtr cancel_flag,
                                       cc::PaintCanvas* canvas,
                                       const Snapshot& snapshot,
                                       const gfx::Rect& rect,
                                       float total_scale,
                                       bool scale_pending,
                                       bool scrolling);
  const Snapshot MakeSnapshot(CancelFlagPtr cancel_flag, const gfx::Rect& rect);
  void PaintTile(CancelFlagPtr cancel_flag,
                 lok::Document* document,
                 unsigned int tile_index);
  void SetYPosition(float y);
  void Resize(long width_twips, long heigh_twips);
  void Resize(long width_twips, long heigh_twips, float scale);
  void ResetScale(float scale);
  TileRange LimitIndex(int y_pos, unsigned int view_height);
  std::vector<TileRange> InvalidRangesRemaining(
      std::vector<TileRange> tile_ranges);
  std::vector<TileRange> ClipRanges(std::vector<TileRange> ranges,
                                    TileRange range_limit);

 private:
  unsigned int CoordToIndex(unsigned int x, unsigned int y) {
    return CoordToIndex(columns_, x, y);
  };

  static unsigned int CoordToIndex(int columns,
                                   unsigned int x,
                                   unsigned int y) {
    return y * columns + x;
  };

  std::pair<unsigned int, unsigned int> IndexToCoord(unsigned int index) {
    unsigned int row = index / columns_;
    unsigned int column = index % columns_;
    return std::pair<unsigned int, unsigned int>(column, row);
  };

  unsigned long NextPoolIndex() {
    return current_pool_index_.fetch_add(1, std::memory_order_relaxed) %
           kPoolSize;
  }

  void InvalidatePoolTile(size_t pool_index) {
    unsigned int tile_index = pool_index_to_tile_index_[pool_index];

    // tile is already invalid
    if (tile_index == kInvalidTileIndex)
      return;

    valid_tile_.Reset(tile_index);
    pool_index_to_tile_index_[pool_index] = kInvalidTileIndex;
  }

  uint8_t* GetPoolBuffer(size_t pool_index) {
    return &pool_buffer_[pool_index * kBufferStride];
  }

  // returns true if the tile resides in the pool, false otherwise
  bool TileToPoolIndex(unsigned int tile_index, size_t* pool_index) {
    size_t result = *pool_index = tile_index % kPoolSize;
    return result < kPoolSize &&
           pool_index_to_tile_index_[result] == tile_index;
  }

  struct RowLimit {
    unsigned int start;
    unsigned int end;
  };

  RowLimit LimitRange(int y_pos, unsigned int view_height);

  unsigned int columns_;
  unsigned int rows_;
  float scale_;

  long doc_width_twips_;
  long doc_height_twips_;
  float doc_width_scaled_px_;
  float doc_height_scaled_px_;

  AtomicBitset valid_tile_;

  // ring pool (in order to prevent OOM crash on invididual tile allocations)

  // Allocated size of the buffer pool
  // TODO: handle memory pressure using base/memory/MemoryPressureListener
  // 256MiB should be sufficient to display an 8K display twice, so should be
  // fine for now?
  static constexpr size_t kPoolAllocatedSize = 256 * 1024 * 1024;
  static constexpr size_t kPoolAligned = 4096;
  static constexpr size_t kBytesPerPx = 4;  // both color types are 32-bit
  static constexpr unsigned int kInvalidTileIndex =
      std::numeric_limits<unsigned int>::max();

  std::shared_ptr<uint8_t[]> pool_buffer_ = nullptr;
  static constexpr size_t kBufferStride =
      kTileSizePx * kTileSizePx * kBytesPerPx;
  static constexpr size_t kPoolSize = kPoolAllocatedSize / kBufferStride - 1;

  unsigned int pool_index_to_tile_index_[kPoolSize];
  std::array<cc::PaintImage, kPoolSize> pool_paint_images_;

  std::atomic<unsigned long long> current_pool_index_ = 0;

  // scroll position
  int y_pos_ = 0;
  bool in_paint_ = false;
};
}  // namespace electron::office

#endif  // !OFFICE_LOK_TILEBUFFER_H_
