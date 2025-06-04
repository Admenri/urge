// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_LAYOUT_VERTEX_LAYOUT_H_
#define RENDERER_LAYOUT_VERTEX_LAYOUT_H_

#include "Graphics/GraphicsEngine/interface/InputLayout.h"

#include "base/math/rectangle.h"
#include "base/math/vector.h"
#include "base/memory/allocator.h"

namespace renderer {

struct Vertex {
  base::Vec4 position;
  base::Vec2 texcoord;
  base::Vec4 color;

  Vertex() : position(0), texcoord(0), color(1) {}
  Vertex(const base::Vec4& pos, const base::Vec2& tex, const base::Vec4& cr)
      : position(pos), texcoord(tex), color(cr) {}

  Vertex(const Vertex&) = default;
  Vertex& operator=(const Vertex&) = default;

  static base::Vector<Diligent::LayoutElement> GetLayout();
};

struct SpineVertex {
  base::Vec2 position;
  uint32_t light_color;
  base::Vec2 texcoord;
  uint32_t dark_color;

  SpineVertex() : position(0), light_color(0), texcoord(0), dark_color(0) {}
  SpineVertex(const base::Vec2& pos,
              uint32_t lightcolor,
              const base::Vec2& tex,
              uint32_t darkcolor)
      : position(pos),
        light_color(lightcolor),
        texcoord(tex),
        dark_color(darkcolor) {}

  SpineVertex(const SpineVertex&) = default;
  SpineVertex& operator=(const SpineVertex&) = default;

  static base::Vector<Diligent::LayoutElement> GetLayout();
};

struct Quad {
  Vertex vertices[4];

  Quad() = default;

  static void SetPositionRect(Quad* data, const base::RectF& pos);
  static void SetTexCoordRectNorm(Quad* data, const base::RectF& texcoord);
  static void SetTexCoordRect(Quad* data,
                              const base::RectF& texcoord,
                              const base::Vec2& size);
  static void SetColor(Quad* data, const base::Vec4& color);

  Vertex* data() { return vertices; }
};

struct IndirectParams {
  uint32_t num_vertices;
  uint32_t num_instances;
  uint32_t start_vertex_location;
  uint32_t first_instance_location;

  IndirectParams(uint32_t vertices,
                 uint32_t instances,
                 uint32_t vertex_offset,
                 uint32_t instance_offset)
      : num_vertices(vertices),
        num_instances(instances),
        start_vertex_location(vertex_offset),
        first_instance_location(instance_offset) {}
};

struct IndexedIndirectParams {
  uint32_t num_indices;
  uint32_t num_instances;
  uint32_t first_index_location;
  uint32_t base_vertex;
  uint32_t first_instance_location;

  IndexedIndirectParams(uint32_t indices,
                        uint32_t instances,
                        uint32_t index_offset,
                        uint32_t bvertex,
                        uint32_t instance_offset)
      : num_indices(indices),
        num_instances(instances),
        first_index_location(index_offset),
        base_vertex(bvertex),
        first_instance_location(instance_offset) {}
};

}  // namespace renderer

#endif  //! RENDERER_LAYOUT_VERTEX_LAYOUT_H_
