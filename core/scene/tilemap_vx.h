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

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "core/canvas/bitmap.h"
#include "core/common/common.h"
#include "core/common/definition.h"
#include "core/common/table.h"
#include "core/common/utility.h"
#include "core/render/primitive.h"
#include "core/render/uniform.h"
#include "core/scene/node.h"
#include "core/scene/viewport.h"

namespace urge {

class TilemapVX;

class TilemapVXAbove : public Node {
 public:
  TilemapVXAbove(TilemapVX* parent, RefPtr<Viewport> viewport);

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  TilemapVX* parent_ = nullptr;
};

URGE_BINDING()
class TilemapVX : public Node {
 public:
  URGE_BINDING()
  TilemapVX(URGE_EXCEPT,
            MaybePtr<Viewport> viewport = nullptr,
            int32_t tilesize = 32);
  ~TilemapVX() override;

  URGE_BINDING()
  void Update();

  URGE_BINDING()
  void SetBitmap(int32_t index, MaybePtr<Bitmap> bitmap);
  URGE_BINDING()
  RefPtr<Bitmap> GetBitmap(int32_t index);

  URGE_BINDING()
  ATTR(MaybePtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(bool, Visible) override;
  URGE_BINDING()
  ATTR(int32_t, Z) override;
  URGE_BINDING()
  ATTR(MaybePtr<Table>, MapData);
  URGE_BINDING()
  ATTR(MaybePtr<Table>, FlashData);
  URGE_BINDING()
  ATTR(MaybePtr<Table>, Flags);
  URGE_BINDING()
  ATTR(MaybePtr<Table>, Passages);
  URGE_BINDING()
  ATTR(int32_t, OX);
  URGE_BINDING()
  ATTR(int32_t, OY);

 private:
  friend class TilemapVXAbove;

  struct TileQuad {
    RefPtr<Bitmap> texture;
    RectF source;
    RectF destination;
    glm::vec4 color = glm::vec4(1.0f);
  };

  struct TileDraw {
    RefPtr<Bitmap> texture;
    PrimitiveEmitter::Slot slot;
  };

  struct TileLayer {
    PrimitiveEmitter primitive;
    std::vector<TileDraw> draws;
    bool valid = false;
  };

  enum TileID {
    TILE_A1 = 0,
    TILE_A2,
    TILE_A3,
    TILE_A4,
    TILE_A5,
    TILE_B,
    TILE_C,
    TILE_D,
    TILE_E,
    TILE_NUMS,
  };

  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  void CreateShadowSet(URGE_EXCEPT);
  void UpdateViewport();
  void CollectMapData(bool above, std::vector<TileQuad>* quads);
  void BuildLayer(bool above, TileLayer* layer);
  void DrawLayer(DrawParam param, TileLayer* layer);
  void DrawAboveLayer(DrawParam param);
  bool HasAboveLayer() const { return above_layer_.valid; }

  bool rgss3_style_ = true;

  RefPtr<TilemapVXAbove> above_;
  RefPtr<Bitmap> shadow_texture_;

  RectI render_viewport_ = {};
  glm::vec2 render_offset_ = glm::vec2(0.0f);
  UniformBlockPool::Slot object_slot_ = {};

  int32_t flash_timer_ = 0;
  int32_t flash_opacity_ = 0;
  int32_t frame_index_ = 0;

  int32_t regular_anim_ = 0;
  int32_t waterfall_anim_ = 0;

  RefPtr<Bitmap> bitmaps_[TILE_NUMS] = {};
  RefPtr<Table> map_data_;
  RefPtr<Table> flash_data_;
  RefPtr<Table> flags_;
  int32_t ox_ = 0, oy_ = 0;
  bool xrepeat_ = true, yrepeat_ = true;
  int32_t tilesize_ = 32;

  TileLayer map_layer_;
  TileLayer above_layer_;

  std::vector<TileQuad> scratch_quads_;
};

}
