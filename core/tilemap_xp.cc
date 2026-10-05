// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/tilemap_xp.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/graphics.h"
#include "core/pipeline.h"

namespace urge {

namespace {

//! The four quadrants of an autotile pattern as fractions of the tile size,
//! ordered top left, top right, bottom left, bottom right.
const glm::vec2 kAutotileSrcRegular[48][4] = {
    {{1.0f, 2.0f}, {1.5f, 2.0f}, {1.0f, 2.5f}, {1.5f, 2.5f}},
    {{2.0f, 0.0f}, {1.5f, 2.0f}, {1.0f, 2.5f}, {1.5f, 2.5f}},
    {{1.0f, 2.0f}, {2.5f, 0.0f}, {1.0f, 2.5f}, {1.5f, 2.5f}},
    {{2.0f, 0.0f}, {2.5f, 0.0f}, {1.0f, 2.5f}, {1.5f, 2.5f}},
    {{1.0f, 2.0f}, {1.5f, 2.0f}, {1.0f, 2.5f}, {2.5f, 0.5f}},
    {{2.0f, 0.0f}, {1.5f, 2.0f}, {1.0f, 2.5f}, {2.5f, 0.5f}},
    {{1.0f, 2.0f}, {2.5f, 0.0f}, {1.0f, 2.5f}, {2.5f, 0.5f}},
    {{2.0f, 0.0f}, {2.5f, 0.0f}, {1.0f, 2.5f}, {2.5f, 0.5f}},
    {{1.0f, 2.0f}, {1.5f, 2.0f}, {2.0f, 0.5f}, {1.5f, 2.5f}},
    {{2.0f, 0.0f}, {1.5f, 2.0f}, {2.0f, 0.5f}, {1.5f, 2.5f}},
    {{1.0f, 2.0f}, {2.5f, 0.0f}, {2.0f, 0.5f}, {1.5f, 2.5f}},
    {{2.0f, 0.0f}, {2.5f, 0.0f}, {2.0f, 0.5f}, {1.5f, 2.5f}},
    {{1.0f, 2.0f}, {1.5f, 2.0f}, {2.0f, 0.5f}, {2.5f, 0.5f}},
    {{2.0f, 0.0f}, {1.5f, 2.0f}, {2.0f, 0.5f}, {2.5f, 0.5f}},
    {{1.0f, 2.0f}, {2.5f, 0.0f}, {2.0f, 0.5f}, {2.5f, 0.5f}},
    {{2.0f, 0.0f}, {2.5f, 0.0f}, {2.0f, 0.5f}, {2.5f, 0.5f}},
    {{0.0f, 2.0f}, {0.5f, 2.0f}, {0.0f, 2.5f}, {0.5f, 2.5f}},
    {{0.0f, 2.0f}, {2.5f, 0.0f}, {0.0f, 2.5f}, {0.5f, 2.5f}},
    {{0.0f, 2.0f}, {0.5f, 2.0f}, {0.0f, 2.5f}, {2.5f, 0.5f}},
    {{0.0f, 2.0f}, {2.5f, 0.0f}, {0.0f, 2.5f}, {2.5f, 0.5f}},
    {{1.0f, 1.0f}, {1.5f, 1.0f}, {1.0f, 1.5f}, {1.5f, 1.5f}},
    {{1.0f, 1.0f}, {1.5f, 1.0f}, {1.0f, 1.5f}, {2.5f, 0.5f}},
    {{1.0f, 1.0f}, {1.5f, 1.0f}, {2.0f, 0.5f}, {1.5f, 1.5f}},
    {{1.0f, 1.0f}, {1.5f, 1.0f}, {2.0f, 0.5f}, {2.5f, 0.5f}},
    {{2.0f, 2.0f}, {2.5f, 2.0f}, {2.0f, 2.5f}, {2.5f, 2.5f}},
    {{2.0f, 2.0f}, {2.5f, 2.0f}, {2.0f, 0.5f}, {2.5f, 2.5f}},
    {{2.0f, 0.0f}, {2.5f, 2.0f}, {2.0f, 2.5f}, {2.5f, 2.5f}},
    {{2.0f, 0.0f}, {2.5f, 2.0f}, {2.0f, 0.5f}, {2.5f, 2.5f}},
    {{1.0f, 3.0f}, {1.5f, 3.0f}, {1.0f, 3.5f}, {1.5f, 3.5f}},
    {{2.0f, 0.0f}, {1.5f, 3.0f}, {1.0f, 3.5f}, {1.5f, 3.5f}},
    {{1.0f, 3.0f}, {2.5f, 0.0f}, {1.0f, 3.5f}, {1.5f, 3.5f}},
    {{2.0f, 0.0f}, {2.5f, 0.0f}, {1.0f, 3.5f}, {1.5f, 3.5f}},
    {{0.0f, 2.0f}, {2.5f, 2.0f}, {0.0f, 2.5f}, {2.5f, 2.5f}},
    {{1.0f, 1.0f}, {1.5f, 1.0f}, {1.0f, 3.5f}, {1.5f, 3.5f}},
    {{0.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 1.5f}, {0.5f, 1.5f}},
    {{0.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 1.5f}, {2.5f, 0.5f}},
    {{2.0f, 1.0f}, {2.5f, 1.0f}, {2.0f, 1.5f}, {2.5f, 1.5f}},
    {{2.0f, 1.0f}, {2.5f, 1.0f}, {2.0f, 0.5f}, {2.5f, 1.5f}},
    {{2.0f, 3.0f}, {2.5f, 3.0f}, {2.0f, 3.5f}, {2.5f, 3.5f}},
    {{2.0f, 0.0f}, {2.5f, 3.0f}, {2.0f, 3.5f}, {2.5f, 3.5f}},
    {{0.0f, 3.0f}, {0.5f, 3.0f}, {0.0f, 3.5f}, {0.5f, 3.5f}},
    {{0.0f, 3.0f}, {2.5f, 0.0f}, {0.0f, 3.5f}, {0.5f, 3.5f}},
    {{0.0f, 1.0f}, {2.5f, 1.0f}, {0.0f, 1.5f}, {2.5f, 1.5f}},
    {{0.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 3.5f}, {0.5f, 3.5f}},
    {{0.0f, 3.0f}, {2.5f, 3.0f}, {0.0f, 3.5f}, {2.5f, 3.5f}},
    {{2.0f, 1.0f}, {2.5f, 1.0f}, {2.0f, 3.5f}, {2.5f, 3.5f}},
    {{0.0f, 1.0f}, {2.5f, 1.0f}, {0.0f, 3.5f}, {2.5f, 3.5f}},
    {{0.0f, 0.0f}, {0.5f, 0.0f}, {0.0f, 0.5f}, {0.5f, 0.5f}},
};

}  // namespace

TilemapXPAbove::TilemapXPAbove(TilemapXP* parent,
                               RefPtr<Viewport> viewport,
                               const ZValue& z,
                               int32_t id)
    : Node(viewport, z), parent_(parent), id_(id) {}

bool TilemapXPAbove::Prepare(DrawParam param) {
  return parent_ && parent_->HasAboveLayer(id_);
}

bool TilemapXPAbove::DoDraw(DrawParam param) {
  parent_->DrawAboveLayer(param, id_);
  return false;
}

TilemapXP::TilemapXP(RefPtr<Viewport> viewport) : Node(viewport, ZValue()) {
  Node::SetupTrait(this);
}

TilemapXP::~TilemapXP() {
  Disposable::Dispose();
}

void TilemapXP::Update() {
  int32_t max_anim_index = 1;
  for (auto& it : autotiles_)
    max_anim_index *= std::max(1, it.frames);
  anim_index_ = ++anim_index_ % (max_anim_index * 16);
}

void TilemapXP::SetTileset(RefPtr<Bitmap> bitmap) {
  tileset_ = bitmap;
}

RefPtr<Bitmap> TilemapXP::GetTileset() {
  return tileset_;
}

void TilemapXP::SetAutotile(int32_t index, RefPtr<Bitmap> bitmap) {
  autotiles_[index].texture = bitmap;
  if (!bitmap) {
    autotiles_[index].frames = 0;
  } else if (bitmap->size().y > tilesize_) {
    autotiles_[index].frames = bitmap->size().x / (tilesize_ * 3);
  } else {
    autotiles_[index].frames = bitmap->size().x / tilesize_;
  }
}

RefPtr<Bitmap> TilemapXP::GetAutotile(int32_t index) {
  return autotiles_[index].texture;
}

ATTR_DEF(TilemapXP, RefPtr<Viewport>, Viewport) {
  // Every above layer is a node of its own, so all of them follow the viewport
  // which is set here.
  if (value.has_value())
    for (auto& it : aboves_)
      it->Attr_Parent(*value);

  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(TilemapXP, bool, Visible) {
  // Mirrored onto the above layers, so a hidden tilemap disappears completely.
  if (value.has_value())
    for (auto& it : aboves_)
      it->Attr_Visible(*value);
  return Node::Attr_Visible(value);
}

ATTR_DEF(TilemapXP, int32_t, Z) {
  // Not settable in RGSS1: the layers derive their Z from the rows of the
  // screen they stand for, see UpdateOrder().
  if (value.has_value())
    return std::nullopt;
  return Node::Attr_Z(value);
}

ATTR_DEF(TilemapXP, RefPtr<Table>, MapData) {
  if (value.has_value()) {
    map_data_ = *value;
    return std::nullopt;
  } else {
    return map_data_;
  }
}

ATTR_DEF(TilemapXP, RefPtr<Table>, FlashData) {
  if (value.has_value()) {
    flash_data_ = *value;
    return std::nullopt;
  } else {
    return flash_data_;
  }
}

ATTR_DEF(TilemapXP, RefPtr<Table>, Priorities) {
  if (value.has_value()) {
    priorities_ = *value;
    return std::nullopt;
  } else {
    return priorities_;
  }
}

ATTR_DEF(TilemapXP, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(TilemapXP, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

void TilemapXP::DisposeObject() {
  Node::DisposeObject();

  aboves_.clear();
  tileset_.reset();
  for (auto& it : autotiles_)
    it.texture.reset();

  ground_layer_.primitive.Reset();
  ground_layer_.draws.clear();
  for (auto& layer : above_layers_)
    layer.primitive.Reset();
  above_layers_.clear();
}

bool TilemapXP::Prepare(DrawParam param) {
  // The region a tilemap draws follows from its viewport, so it is read before
  // the layers are built, which the children above do in their own prepare
  // stage, see TilemapXPAbove::Prepare().
  UpdateViewport();
  UpdateAboves();
  UpdateOrder();
  BuildLayers();

  // The transform of the tiles is the identity, so every layer shares the slot.
  const bool any_layer = ground_layer_.valid;
  bool any_above = false;
  for (const auto& layer : above_layers_)
    any_above = any_above || layer.valid;

  object_slot_ = {};
  if (any_layer || any_above) {
    UniformManager& uniforms = UniformManager::Get();
    const ObjectData object_data = {glm::mat4(1.0f)};
    object_slot_ = uniforms.object_uniforms().Acquire(object_data);
  }

  return true;
}

bool TilemapXP::DoDraw(DrawParam param) {
  DrawLayer(param, ground_layer_);
  return false;
}

void TilemapXP::UpdateViewport() {
  auto viewport = Attr_Viewport();
  int32_t viewport_ox = 0, viewport_oy = 0;
  int32_t viewport_width = 0, viewport_height = 0;
  if (viewport.has_value() && viewport.value()) {
    auto rect = viewport.value()->Attr_Rect();
    viewport_ox = viewport.value()->Attr_OX().value();
    viewport_oy = viewport.value()->Attr_OY().value();
    viewport_width = rect.value()->data.width;
    viewport_height = rect.value()->data.height;
  } else {
    // A tilemap without a viewport covers the whole screen, which has no origin
    // of its own, so both origins stay at zero.
    viewport_width = Graphics::Get().Width();
    viewport_height = Graphics::Get().Height();
  }

  const int32_t tilemap_real_ox = ox_ + viewport_ox;
  const int32_t tilemap_real_oy = oy_ + viewport_oy;

  // Quad parsing viewport
  render_viewport_.x = tilemap_real_ox / tilesize_;
  render_viewport_.y = tilemap_real_oy / tilesize_ - 1;
  render_viewport_.width =
      (viewport_width / tilesize_) + !!(viewport_width % tilesize_) + 1;
  render_viewport_.height =
      (viewport_height / tilesize_) + !!(viewport_height % tilesize_) + 2;

  // Rendering offset
  const int32_t display_offset_x = tilemap_real_ox % tilesize_;
  const int32_t display_offset_y = tilemap_real_oy % tilesize_;
  render_offset_ = glm::vec2(static_cast<float>(-display_offset_x),
                             static_cast<float>(-display_offset_y));
  render_offset_.y -= static_cast<float>(tilesize_);
}

void TilemapXP::UpdateAboves() {
  auto viewport = Attr_Viewport();
  int32_t viewport_height = 0;
  if (viewport.has_value() && viewport.value()) {
    viewport_height = viewport.value()->Attr_Rect().value()->data.height;
  } else {
    viewport_height = Graphics::Get().Height();
  }

  // RGSS1 Z rule: priority 0 at Z 0, priority 1 top edge at Z 64, +32 per step.
  const int32_t above_layers_count = (viewport_height / tilesize_) +
                                     !!(viewport_height % tilesize_) + 2 +
                                     kMaxPriorities;
  if (above_layers_count == static_cast<int32_t>(aboves_.size()))
    return;

  aboves_.clear();
  aboves_.reserve(above_layers_count);
  RefPtr<Viewport> viewport_ref =
      viewport.has_value() ? viewport.value() : nullptr;
  for (int32_t i = 0; i < above_layers_count; ++i) {
    auto above_node =
        MakeRefCounted<TilemapXPAbove>(this, viewport_ref, ZValue(), i);
    aboves_.push_back(std::move(above_node));
  }
}

void TilemapXP::UpdateOrder() {
  for (int32_t i = 0; i < static_cast<int32_t>(aboves_.size()); ++i) {
    // i -> 1 [2  3  4   5   6]  7
    // z -> 32 64 96 128 160 192 224
    const int32_t layer_order = 32 * (render_viewport_.y + i + 2) - oy_;
    aboves_[i]->Attr_Z(layer_order);
  }
}

void TilemapXP::ParseTiles(std::vector<TileQuad>* ground,
                           std::vector<std::vector<TileQuad>>* aboves) {
  ground->clear();
  aboves->clear();
  aboves->resize(aboves_.size());

  auto push_quad = [](const RefPtr<Bitmap>& texture,
                      std::vector<TileQuad>* target, const RectF& source,
                      const RectF& destination) {
    TileQuad quad;
    quad.texture = texture;
    quad.source = source;
    quad.destination = destination;
    target->push_back(quad);
  };

  auto set_autotile_pos = [&](RectF& pos, int32_t index) {
    switch (index) {
      case 0:  // Left Top
        break;
      case 1:  // Right Top
        pos.x += tilesize_ / 2.0f;
        break;
      case 2:  // Left Bottom
        pos.y += tilesize_ / 2.0f;
        break;
      case 3:  // Right Bottom
        pos.x += tilesize_ / 2.0f;
        pos.y += tilesize_ / 2.0f;
        break;
      default:
        break;
    }
  };

  auto get_priority = [&](int16_t tile_id) -> int32_t {
    if (!priorities_ || tile_id >= priorities_->Xsize())
      return 0;

    int16_t value = priorities_->Get(tile_id, 0, 0);
    if (value > kMaxPriorities)
      return -1;

    return value;
  };

  auto process_autotile = [&](int32_t x, int32_t y, int16_t tile_id,
                              std::vector<TileQuad>* target) {
    // Autotile (0-7)
    const int32_t autotile_id = tile_id / 48 - 1;
    // Pattern (0-47)
    const int32_t pattern_id = tile_id % 48;

    if (autotile_id < 0 || autotile_id >= 7)
      return;

    // Autotile invalid check
    const Autotile& autotile = autotiles_[autotile_id];
    if (!Disposable::Check(autotile.texture))
      return;

    const int32_t frames = std::max(1, autotile.frames);
    const int32_t frame = anim_index_ % frames;

    // Generate from autotile type
    if (autotile.texture->size().y >= tilesize_ * 4) {
      const glm::vec2* autotile_src_pos = kAutotileSrcRegular[pattern_id];
      for (int32_t i = 0; i < 4; ++i) {
        RectF tex_src;
        tex_src.x =
            frame * tilesize_ * 3 + autotile_src_pos[i].x * tilesize_ + 0.5f;
        tex_src.y = autotile_src_pos[i].y * tilesize_ + 0.5f;
        tex_src.width = 0.5f * tilesize_ - 1.0f;
        tex_src.height = 0.5f * tilesize_ - 1.0f;

        RectF chunk_pos(x * tilesize_, y * tilesize_, tilesize_ / 2.0f,
                        tilesize_ / 2.0f);
        set_autotile_pos(chunk_pos, i);

        push_quad(autotile.texture, target, tex_src, chunk_pos);
      }
    } else if (autotile.texture->size().y <= tilesize_) {
      const RectF single_tex(frame * tilesize_ + 0.5f, 0.5f, tilesize_ - 1.0f,
                             tilesize_ - 1.0f);
      const RectF single_pos(x * tilesize_, y * tilesize_, tilesize_,
                             tilesize_);

      push_quad(autotile.texture, target, single_tex, single_pos);
    }
  };

  auto value_wrap = [&](int32_t value, int32_t range) {
    int32_t res = value % range;
    return res < 0 ? res + range : res;
  };

  auto get_wrap_data = [&](RefPtr<Table> t, int32_t x, int32_t y,
                           int32_t z) -> int16_t {
    if (!t)
      return 0;

    auto tile_x = xrepeat_ ? value_wrap(x, t->Xsize()) : x;
    auto tile_y = yrepeat_ ? value_wrap(y, t->Ysize()) : y;

    if (!xrepeat_ && (x < 0 || x >= t->Xsize()))
      return 0;
    if (!yrepeat_ && (y < 0 || y >= t->Ysize()))
      return 0;

    return t->Get(tile_x, tile_y, z);
  };

  auto process_tile = [&](int32_t x, int32_t y, int32_t z) {
    const int16_t tile_id = get_wrap_data(map_data_, x + render_viewport_.x,
                                          y + render_viewport_.y, z);

    if (tile_id < 48)
      return;

    const int32_t priority = get_priority(tile_id);
    if (priority == -1)
      return;

    std::vector<TileQuad>* target;
    if (!priority) {
      // Ground layer
      target = ground;
    } else {
      // Above multi layers
      const size_t index = static_cast<size_t>(y + priority);
      if (index >= aboves->size())
        return;
      target = &(*aboves)[index];
    }

    if (tile_id < 48 * 8)
      return process_autotile(x, y, tile_id, target);

    if (Disposable::Check(tileset_)) {
      const int32_t tileset_id = tile_id - 48 * 8;
      const int32_t tile_x = tileset_id % 8;
      const int32_t tile_y = tileset_id / 8;

      const RectF quad_tex(tile_x * tilesize_ + 0.5f, tile_y * tilesize_ + 0.5f,
                           tilesize_ - 1.0f, tilesize_ - 1.0f);
      const RectF quad_pos(x * tilesize_, y * tilesize_, tilesize_, tilesize_);

      push_quad(tileset_, target, quad_tex, quad_pos);
    }
  };

  if (!map_data_)
    return;

  for (int32_t x = 0; x < render_viewport_.width; ++x)
    for (int32_t y = 0; y < render_viewport_.height; ++y)
      for (int32_t z = 0; z < map_data_->Zsize(); ++z)
        process_tile(x, y, z);
}

void TilemapXP::BuildLayers() {
  std::vector<TileQuad> ground;
  std::vector<std::vector<TileQuad>> aboves;
  ParseTiles(&ground, &aboves);

  // Bitmap contents are stored premultiplied, so the tile color scales all four
  // channels of its vertices.
  const glm::vec4 color(1.0f);

  auto build = [&](const std::vector<TileQuad>& quads, TileLayer* layer) {
    layer->draws.clear();
    layer->valid = false;
    layer->primitive.Clear();

    size_t index = 0;
    while (index < quads.size()) {
      const RefPtr<Bitmap>& texture = quads[index].texture;

      // A tile whose bitmap is gone is skipped, the run continues without it
      if (!Disposable::Check(texture)) {
        ++index;
        continue;
      }

      const glm::vec2 texture_size(static_cast<float>(texture->size().x),
                                   static_cast<float>(texture->size().y));

      // The tiles reading the same bitmap travel in one batch, so a layer costs
      // one draw per bitmap it uses.
      layer->primitive.BeginQuad().Color4f(color);
      while (index < quads.size() && quads[index].texture == texture) {
        RectF dest = quads[index].destination;
        dest.x += render_offset_.x;
        dest.y += render_offset_.y;

        layer->primitive.Rect(dest,
                              MakeNorm(quads[index].source, texture_size));
        ++index;
      }

      const PrimitiveEmitter::Slot run = layer->primitive.End();
      if (run.count) {
        TileDraw draw;
        draw.texture = texture;
        draw.slot = run;
        layer->draws.push_back(std::move(draw));
      }
    }

    if (layer->draws.empty())
      return;

    layer->primitive.Upload();
    layer->valid = layer->primitive.buffer() != nullptr;
    if (!layer->valid)
      layer->draws.clear();
  };

  // Built in the order they are drawn in, so the vertices of the frame sit in
  // the buffers in that order.
  build(ground, &ground_layer_);

  above_layers_.resize(aboves.size());
  for (size_t i = 0; i < aboves.size(); ++i)
    build(aboves[i], &above_layers_[i]);
}

void TilemapXP::DrawLayer(DrawParam param, const TileLayer& layer) {
  if (!layer.valid || layer.draws.empty() ||
      object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return;

  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

  // The tiles are placed by the vertex position they were emitted with, so the
  // object set carries the identity, see BuildLayers().
  param->pass.SetPipeline(ShaderSet::Get().state.tilemap.texture_dynamic_pma);
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
  param->pass.SetVertexBuffer(0, layer.primitive.buffer(), 0, WGPU_WHOLE_SIZE);

  for (const TileDraw& draw : layer.draws) {
    param->pass.SetBindGroup(2, draw.texture->texture_group(), 0, nullptr);
    param->pass.Draw(draw.slot.count, 1, draw.slot.first, 0);
  }
}

void TilemapXP::DrawAboveLayer(DrawParam param, int32_t id) {
  if (id < 0 || static_cast<size_t>(id) >= above_layers_.size())
    return;
  DrawLayer(param, above_layers_[id]);
}

bool TilemapXP::HasAboveLayer(int32_t id) const {
  return id >= 0 && static_cast<size_t>(id) < above_layers_.size() &&
         above_layers_[id].valid;
}

}  // namespace urge
