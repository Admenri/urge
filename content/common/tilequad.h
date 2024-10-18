// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TILEQUAD_H_
#define CONTENT_COMMON_TILEQUAD_H_

#include "base/math/rectangle.h"

namespace content {

inline int QuadTileCount(int tile, int dest) {
  if (!tile || !dest)
    return 0;

  return dest / tile + (dest % tile ? 1 : 0);
}

inline int QuadTileCount2D(int tile_width,
                           int tile_height,
                           int dest_width,
                           int dest_height) {
  return QuadTileCount(tile_width, dest_width) *
         QuadTileCount(tile_height, dest_height);
}

template <typename Vertex>
int BuildTileH(Vertex* vertex,
               const base::Rect& src_rect,
               int width,
               int x,
               int y) {
  if (width <= 0)
    return 0;

  int full_size = width / src_rect.width;
  int part_size = width % src_rect.width;

  base::Rect dest_rect(x, y, src_rect.width, src_rect.height);
  for (int i = 0; i < full_size; ++i) {
    Vertex* vert = &vertex[i * 4];

    renderer::GeometryVertexLayout::SetTexcoord(vert, src_rect);
    renderer::GeometryVertexLayout::SetPosition(vert, dest_rect);

    dest_rect.x += src_rect.width;
  }

  base::Rect process_src_rect(src_rect);
  if (part_size) {
    Vertex* vert = &vertex[full_size * 4];

    process_src_rect.width = part_size;
    dest_rect.width = part_size;

    renderer::GeometryVertexLayout::SetTexcoord(vert, process_src_rect);
    renderer::GeometryVertexLayout::SetPosition(vert, dest_rect);
  }

  return full_size + (part_size ? 1 : 0);
}

template <typename Vertex>
int BuildTileV(Vertex* vertex,
               const base::Rect& src_rect,
               int height,
               int x,
               int y) {
  if (height <= 0)
    return 0;

  int full_size = height / src_rect.height;
  int part_size = height % src_rect.height;

  base::Rect dest_rect(x, y, src_rect.width, src_rect.height);
  for (int i = 0; i < full_size; ++i) {
    Vertex* vert = &vertex[i * 4];

    renderer::GeometryVertexLayout::SetTexcoord(vert, src_rect);
    renderer::GeometryVertexLayout::SetPosition(vert, dest_rect);

    dest_rect.y += src_rect.height;
  }

  base::Rect process_src_rect(src_rect);
  if (part_size) {
    Vertex* vert = &vertex[full_size * 4];

    process_src_rect.height = part_size;
    dest_rect.height = part_size;

    renderer::GeometryVertexLayout::SetTexcoord(vert, process_src_rect);
    renderer::GeometryVertexLayout::SetPosition(vert, dest_rect);
  }

  return full_size + (part_size ? 1 : 0);
}

template <typename T>
int BuildTiles(T* vertex,
               const base::Rect& src_rect,
               const base::Rect& dest_rect) {
  int width = dest_rect.width;
  int height = dest_rect.height;

  if (width <= 0 || height <= 0)
    return 0;

  int full_size = height / src_rect.height;
  int part_size = height % src_rect.height;

  int row_tile_size = QuadTileCount(src_rect.width, width);
  int quad_count = 0;

  int v = 0;
  int ox = dest_rect.x;
  int oy = dest_rect.y;
  for (int i = 0; i < full_size; ++i) {
    quad_count += BuildTileH<T>(&vertex[v], src_rect, width, ox, oy);

    v += row_tile_size * 4;
    oy += src_rect.height;
  }

  if (part_size) {
    base::Rect part_src_rect = src_rect;
    part_src_rect.height = part_size;

    quad_count += BuildTileH<T>(&vertex[v], part_src_rect, width, ox, oy);
  }

  return quad_count;
}

}  // namespace content

#endif  // !CONTENT_COMMON_TILEQUAD_H_