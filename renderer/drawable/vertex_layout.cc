// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/drawable/vertex_layout.h"

namespace renderer {

std::vector<Diligent::LayoutElement> GeometryVertexLayout::GetLayout() {
  std::vector<Diligent::LayoutElement> elems = {
      /* Position */
      Diligent::LayoutElement{0, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
      /* TexCoord */
      Diligent::LayoutElement{1, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
      /* Color */
      Diligent::LayoutElement{2, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
  };

  return elems;
}

int GeometryVertexLayout::SetTexPos(Data* data,
                                    const base::RectF& tex,
                                    const base::RectF& pos) {
  SetTexcoord(data, tex);
  SetPosition(data, pos);
  return 1;
}

void GeometryVertexLayout::SetPosition(Data* data, const base::RectF& pos) {
  data[0].position = base::Vec4(pos.x, pos.y, 0.0f, 1.0f);
  data[1].position = base::Vec4(pos.x + pos.width, pos.y, 0.0f, 1.0f);
  data[2].position =
      base::Vec4(pos.x + pos.width, pos.y + pos.height, 0.0f, 1.0f);
  data[3].position = base::Vec4(pos.x, pos.y + pos.height, 0.0f, 1.0f);
}

void GeometryVertexLayout::SetTexcoord(Data* data, const base::RectF& tex) {
  data[0].texcoord = base::Vec2(tex.x, tex.y);
  data[1].texcoord = base::Vec2(tex.x + tex.width, tex.y);
  data[2].texcoord = base::Vec2(tex.x + tex.width, tex.y + tex.height);
  data[3].texcoord = base::Vec2(tex.x, tex.y + tex.height);
}

void GeometryVertexLayout::SetColor(Data* data,
                                    const base::Vec4& color,
                                    int index) {
  if (index >= 0) {
    data[index].color = color;
    return;
  }

  for (int i = 0; i < 4; ++i)
    data[i].color = color;
}

}  // namespace renderer
