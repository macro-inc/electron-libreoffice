// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "renderer_transferable.h"
#include "paint_manager.h"

namespace electron::office {
RendererTransferable::RendererTransferable(
    scoped_refptr<TileBuffer>&& tile_buffer,
    std::unique_ptr<PaintManager>&& paint_manager,
    Snapshot snapshot,
    std::vector<gfx::Rect> page_rects_cached,
    int first_intersect,
    int last_intersect,
    std::string&& last_cursor,
    float zoom)
    : tile_buffer(std::move(tile_buffer)),
      paint_manager(std::move(paint_manager)),
      snapshot(std::move(snapshot)),
      page_rects(std::move(page_rects_cached)),
      first_intersect(first_intersect),
      last_intersect(last_intersect),
      last_cursor_rect(std::move(last_cursor)),
      zoom(zoom) {}

RendererTransferable::RendererTransferable() = default;
RendererTransferable::~RendererTransferable() = default;
RendererTransferable& RendererTransferable::operator=(
    RendererTransferable&&) noexcept = default;
RendererTransferable::RendererTransferable(RendererTransferable&&) noexcept =
    default;
}  // namespace electron::office
