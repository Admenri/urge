// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_LAYOUT_VERTEX_LAYOUT_H_
#define RENDERER_LAYOUT_VERTEX_LAYOUT_H_

#include "Graphics/GraphicsEngine/interface/InputLayout.h"

#include "base/math/rectangle.h"
#include "base/math/vector.h"

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

  static std::vector<Diligent::LayoutElement> GetLayout();
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

  static std::vector<Diligent::LayoutElement> GetLayout();
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

}  // namespace renderer

#endif  //! RENDERER_LAYOUT_VERTEX_LAYOUT_H_
