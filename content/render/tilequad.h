// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_TILEQUAD_H_
#define CONTENT_RENDER_TILEQUAD_H_

#include "base/math/rectangle.h"
#include "renderer/layout/vertex_layout.h"

namespace content {

enum class TileAxis {
  HORIZONTAL = 0,
  VERTICAL,
};

// Get required quadangle's tile count.
int32_t CalculateQuadTileCount(int32_t tile, int32_t dest);

// Axis direction repeat tile generator:
// (main_pos, cross_pos) -> o|---- main_size ----|
//   o <- (cross_pos, main_pos)
//  ---
//   |
//   | main_size
//   |
//  ---
int32_t BuildTilesAlongAxis(TileAxis axis,
                            const base::Rect& src_rect,
                            const base::Vec2i& dest_pos,
                            const base::Vec4& color,
                            int32_t main_axis_size,
                            const base::Vec2i& texture_size,
                            renderer::Quad* quads);

// Region tile generator:
//  Fill size|src_rect| tiles in target|dest_rect|.
int32_t BuildTiles(const base::Rect& src_rect,
                   const base::Rect& dest_rect,
                   const base::Vec4& color,
                   const base::Vec2i& texture_size,
                   renderer::Quad* quads);

}  // namespace content

#endif  //! CONTENT_RENDER_TILEQUAD_H_
