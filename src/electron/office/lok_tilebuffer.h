// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_LOK_TILEBUFFER_H_
#define OFFICE_LOK_TILEBUFFER_H_

#include <map>
#include <unordered_set>
#include "third_party/libreofficekit/LibreOfficeKit.hxx"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace electron::office {
class TileBuffer {
 public:
  static constexpr int kTileSize = 256;
  explicit TileBuffer(lok::Document* document,
                      float scale = 1.0f,
                      int part = 0);
  ~TileBuffer();

  TileBuffer(const TileBuffer&);

  void InvalidateTile(int column, int row);
  void InvalidateTilesInRect(const gfx::RectF& rect);
  void InvalidateTilesInTwipRect(const gfx::Rect& rect_twips);
  void InvalidateAllTiles();
  void PaintInvalidTiles(SkCanvas& canvas, const gfx::RectF& rect);

  class Tile {
   public:
    explicit Tile(const SkImageInfo& info);
    ~Tile();

   private:
    friend TileBuffer;
    uint8_t* pixels_;
    SkBitmap bitmap_;
  };

 private:
  void PaintTile(int x, int y);
  inline int CoordToIndex(int x, int y) { return y * columns_ + x; };

  lok::Document* document_;
  std::map<int, Tile> tiles_;
  std::unordered_set<int> valid_tile_;
  int columns_;
  int rows_;
  float scale_;
  int part_;

  long doc_width_twips_;
  long doc_height_twips_;
  float doc_width_scaled_px_;
  float doc_height_scaled_px_;
  long tile_size_scaled_px_;
  long tile_size_scaled_twips_;

  SkImageInfo image_info_;
};
}  // namespace electron::office

#endif  // !OFFICE_LOK_TILEBUFFER_H_
