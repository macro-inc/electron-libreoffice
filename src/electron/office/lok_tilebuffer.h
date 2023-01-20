// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_LOK_TILEBUFFER_H_
#define OFFICE_LOK_TILEBUFFER_H_

#include <chrono>
#include <map>
#include <unordered_set>
#include "office/lok_callback.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace lok {
class Document;
}

namespace electron::office {
class TileBuffer {
 public:
  // aim for 60fps (~16.67ms per frame)
  static constexpr std::chrono::milliseconds kFrameDeadline{16};
  static constexpr int kTileSizePx = 256;
  static constexpr int kTileSizeTwips = kTileSizePx * lok_callback::kTwipPerPx;
  explicit TileBuffer(lok::Document* document,
                      float scale = 1.0f,
                      int part = 0);
  ~TileBuffer();

  TileBuffer(const TileBuffer&);

  void InvalidateTile(int column, int row);
  void InvalidateTilesInRect(const gfx::RectF& rect);
  void InvalidateTilesInTwipRect(const gfx::Rect& rect_twips);
  void InvalidateAllTiles();
  void PaintInvalidTiles(SkCanvas& canvas,
                         const gfx::Rect& rect,
                         std::chrono::steady_clock::time_point start,
                         std::vector<gfx::Rect>& ready,
                         std::vector<gfx::Rect>& pending);

 private:
  void PaintTile(uint8_t* buffer, int column, int row);
  inline int CoordToIndex(int x, int y) { return y * columns_ + x; };
  inline size_t NextPoolIndex() { return (current_index_ + 1) % pool_size_; }
  inline void InvalidatePoolTile(size_t pool_index) {
    int tile_index = pool_index_to_tile_index_[pool_index];

    // tile is already invalid
    if (tile_index == -1)
      return;

    valid_tile_.erase(tile_index);
    pool_index_to_tile_index_[pool_index] = -1;
  }
  inline uint8_t* GetPoolBuffer(size_t pool_index) {
    return &pool_buffer_[pool_index * pool_buffer_stride_];
  }

  // returns true if the tile resides in the pool, false otherwise
  inline bool TileToPoolIndex(int tile_index, size_t* pool_index) {
    size_t result = *pool_index = tile_index_to_pool_index_[tile_index];
    return result < pool_size_ && tile_index != -1 &&
           pool_index_to_tile_index_[result] == tile_index;
  }

  lok::Document* document_;
  std::unordered_set<int> valid_tile_;

  // may be invalid value, need to validate by checking bounds and
  // pool_index_to_tile_index_ value
  std::map<int, size_t> tile_index_to_pool_index_;
  int columns_;
  int rows_;
  float scale_;
  int part_;

  long doc_width_twips_;
  long doc_height_twips_;
  float doc_width_scaled_px_;
  float doc_height_scaled_px_;
  long tile_size_scaled_px_;

  SkImageInfo image_info_;

  // ring pool (in order to prevent OOM crash on invididual tile allocations)

  // Allocated size of the buffer pool
  // TODO: handle memory pressure using base/memory/MemoryPressureListener
  // 256MiB should be sufficient to display an 8K display twice, so should be
  // fine for now?
  static constexpr size_t kPoolAllocatedSize = 256 * 1024 * 1024;

  std::shared_ptr<uint8_t[]> pool_buffer_ = nullptr;
  size_t pool_buffer_stride_;
  size_t pool_size_;

  std::shared_ptr<SkBitmap[]> pool_bitmaps_ = nullptr;
  std::shared_ptr<int[]> pool_index_to_tile_index_ = nullptr;

  size_t current_index_ = 0;
};
}  // namespace electron::office

#endif  // !OFFICE_LOK_TILEBUFFER_H_
