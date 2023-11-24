// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include "office/lok_tilebuffer.h"

namespace electron::office {

class PaintManager;

struct RendererTransferable {
  scoped_refptr<TileBuffer> tile_buffer;
  std::unique_ptr<PaintManager> paint_manager;
  Snapshot snapshot;
  std::vector<gfx::Rect> page_rects;
  int first_intersect = -1;
  int last_intersect = -1;
  std::string last_cursor_rect;
  float zoom;

  RendererTransferable(scoped_refptr<TileBuffer>&& tile_buffer,
                       std::unique_ptr<PaintManager>&& paint_manager,
                       Snapshot snapshot,
                       std::vector<gfx::Rect> page_rects_cached,
                       int first_intersect,
                       int last_intersect,
                       std::string&& last_cursor,
                       float zoom);

  RendererTransferable();
  ~RendererTransferable();

  // disable copy
  RendererTransferable& operator=(const RendererTransferable&) = delete;
  RendererTransferable(const RendererTransferable&) = delete;

  // enable move
  RendererTransferable& operator=(RendererTransferable&&) noexcept;
  RendererTransferable(RendererTransferable&&) noexcept;
};
}  // namespace electron::office
