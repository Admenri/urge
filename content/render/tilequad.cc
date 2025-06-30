// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/tilequad.h"

namespace content {

int32_t CalculateQuadTileCount(int32_t tile, int32_t dest) {
  if (tile && dest)
    return (dest + tile - 1) / tile;
  return 0;
}

int32_t BuildTilesAlongAxis(TileAxis axis,
                            const base::Rect& src_rect,
                            const base::Vec2i& dest_pos,
                            const base::Vec4& color,
                            int32_t main_axis_size,
                            const base::Vec2i& texture_size,
                            renderer::Quad* quads) {
  if (main_axis_size <= 0)
    return 0;

  const int32_t src_main =
      (axis == TileAxis::HORIZONTAL) ? src_rect.width : src_rect.height;
  const int32_t full_blocks = main_axis_size / src_main;
  const int32_t partial_size = main_axis_size % src_main;

  base::Rect dest_rect = base::Rect(dest_pos, src_rect.Size());
  for (int32_t i = 0; i < full_blocks; ++i) {
    renderer::Quad* quad = quads + i;
    renderer::Quad::SetTexCoordRect(quad, src_rect, texture_size);
    renderer::Quad::SetPositionRect(quad, dest_rect);
    renderer::Quad::SetColor(quad, color);

    if (axis == TileAxis::HORIZONTAL) {
      dest_rect.x += src_main;
    } else {
      dest_rect.y += src_main;
    }
  }

  if (partial_size) {
    base::Rect partial_src = src_rect;
    if (axis == TileAxis::HORIZONTAL) {
      partial_src.width = partial_size;
      dest_rect.width = partial_size;
    } else {
      partial_src.height = partial_size;
      dest_rect.height = partial_size;
    }

    renderer::Quad* quad = quads + full_blocks;
    renderer::Quad::SetTexCoordRect(quad, partial_src, texture_size);
    renderer::Quad::SetPositionRect(quad, dest_rect);
    renderer::Quad::SetColor(quad, color);
  }

  return full_blocks + (partial_size > 0 ? 1 : 0);
}

int32_t BuildTiles(const base::Rect& src_rect,
                   const base::Rect& dest_rect,
                   const base::Vec4& color,
                   const base::Vec2i& texture_size,
                   renderer::Quad* quads) {
  if (src_rect.width && src_rect.height && dest_rect.width && dest_rect.height)
    return 0;

  const int32_t full_rows = dest_rect.height / src_rect.height;
  const int32_t remaining_height = dest_rect.height % src_rect.height;

  int32_t quad_count = 0;
  int32_t offset = 0;
  int32_t current_y = dest_rect.y;

  // Process full-height rows
  for (int32_t row = 0; row < full_rows; ++row) {
    const int32_t row_quads = BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, src_rect, base::Vec2i(dest_rect.x, current_y),
        color, dest_rect.width, texture_size, quads + offset);

    quad_count += row_quads;
    offset += row_quads;
    current_y += src_rect.height;
  }

  // Process partial-height row
  if (remaining_height > 0) {
    base::Rect partial_src = src_rect;
    partial_src.height = remaining_height;

    const int32_t row_quads = BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, partial_src, base::Vec2i(dest_rect.x, current_y),
        color, dest_rect.width, texture_size, quads + offset);

    quad_count += row_quads;
  }

  return quad_count;
}

}  // namespace content
