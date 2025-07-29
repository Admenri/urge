// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/layout/vertex_layout.h"

namespace renderer {

std::vector<Diligent::LayoutElement> Vertex::GetLayout() {
  static std::vector<Diligent::LayoutElement> input_elements = {
      /* Position Vec4 */
      Diligent::LayoutElement{0, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
      /* TexCoord Vec2 */
      Diligent::LayoutElement{1, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
      /* Color Vec4 */
      Diligent::LayoutElement{2, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
  };

  return input_elements;
}

std::vector<Diligent::LayoutElement> SpineVertex::GetLayout() {
  static std::vector<Diligent::LayoutElement> input_elements = {
      /* Position Vec2 */
      Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
      /* LightColor Vec4 */
      Diligent::LayoutElement{1, 0, 4, Diligent::VT_UINT8, Diligent::True},
      /* Texcoord Vec2 */
      Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
      /* DarkColor Vec4 */
      Diligent::LayoutElement{3, 0, 4, Diligent::VT_UINT8, Diligent::True},
  };

  return input_elements;
}

void Quad::SetPositionRect(Quad* data, const base::RectF& pos) {
  int32_t i = 0;
  data->vertices[i++].position = base::Vec4(pos.x, pos.y, 0, 1);
  data->vertices[i++].position = base::Vec4(pos.x + pos.width, pos.y, 0, 1);
  data->vertices[i++].position =
      base::Vec4(pos.x + pos.width, pos.y + pos.height, 0, 1);
  data->vertices[i++].position = base::Vec4(pos.x, pos.y + pos.height, 0, 1);
}

void Quad::SetTexCoordRectNorm(Quad* data, const base::RectF& texcoord) {
  int32_t i = 0;
  data->vertices[i++].texcoord = texcoord.Position();
  data->vertices[i++].texcoord =
      base::Vec2(texcoord.x + texcoord.width, texcoord.y);
  data->vertices[i++].texcoord = texcoord.Position() + texcoord.Size();
  data->vertices[i++].texcoord =
      base::Vec2(texcoord.x, texcoord.y + texcoord.height);
}

void Quad::SetTexCoordRect(Quad* data,
                           const base::RectF& texcoord,
                           const base::Vec2& size) {
  const base::Vec2 tex_pos = texcoord.Position() / size;
  const base::Vec2 tex_size = (texcoord.Position() + texcoord.Size()) / size;

  int32_t i = 0;
  data->vertices[i++].texcoord = tex_pos;
  data->vertices[i++].texcoord = base::Vec2(tex_size.x, tex_pos.y);
  data->vertices[i++].texcoord = tex_size;
  data->vertices[i++].texcoord = base::Vec2(tex_pos.x, tex_size.y);
}

void Quad::SetColor(Quad* data, const base::Vec4& color) {
  int32_t i = 0;
  data->vertices[i++].color = color;
  data->vertices[i++].color = color;
  data->vertices[i++].color = color;
  data->vertices[i++].color = color;
}

}  // namespace renderer
