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

#include "core/bitmap.h"
#include "core/common.h"
#include "core/definition.h"
#include "core/node.h"
#include "core/primitive.h"
#include "core/table.h"
#include "core/uniform.h"
#include "core/utility.h"
#include "core/viewport.h"

namespace urge {

class TilemapXP;

class TilemapXPAbove : public Node {
 public:
  TilemapXPAbove(TilemapXP* parent,
                 RefPtr<Viewport> viewport,
                 const ZValue& z,
                 int32_t id);

 private:
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  TilemapXP* parent_ = nullptr;
  int32_t id_ = 0;
};

URGE_BINDING()
class TilemapXP : public Node {
 public:
  URGE_BINDING()
  TilemapXP(RefPtr<Viewport> viewport = nullptr);
  URGE_BINDING()
  ~TilemapXP() override;

  URGE_BINDING()
  void Update();

  URGE_BINDING(Name : "tileset=")
  void SetTileset(RefPtr<Bitmap> bitmap);
  URGE_BINDING(Name : "tileset")
  RefPtr<Bitmap> GetTileset();

  URGE_BINDING()
  void SetAutotile(int32_t index, RefPtr<Bitmap> bitmap);
  URGE_BINDING()
  RefPtr<Bitmap> GetAutotile(int32_t index);

  URGE_BINDING()
  ATTR(RefPtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(bool, Visible) override;
  URGE_BINDING()
  ATTR(int32_t, Z) override;
  URGE_BINDING()
  ATTR(RefPtr<Table>, MapData);
  URGE_BINDING()
  ATTR(RefPtr<Table>, FlashData);
  URGE_BINDING()
  ATTR(RefPtr<Table>, Priorities);
  URGE_BINDING()
  ATTR(int32_t, OX);
  URGE_BINDING()
  ATTR(int32_t, OY);

 private:
  friend class TilemapXPAbove;

  static constexpr int32_t kMaxPriorities = 5;

  struct TileQuad {
    RefPtr<Bitmap> texture;
    RectF source;
    RectF destination;
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

  struct Autotile {
    RefPtr<Bitmap> texture;
    int32_t frames = 0;
  };

  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  void UpdateViewport();

  void UpdateAboves();

  void UpdateOrder();

  void ParseTiles(std::vector<TileQuad>* ground,
                  std::vector<std::vector<TileQuad>>* aboves);

  void BuildLayers();

  void DrawLayer(DrawParam param, const TileLayer& layer);

  void DrawAboveLayer(DrawParam param, int32_t id);

  bool HasAboveLayer(int32_t id) const;

  std::vector<RefPtr<TilemapXPAbove>> aboves_;

  RectI render_viewport_ = {};

  glm::vec2 render_offset_ = glm::vec2(0.0f);

  UniformBlockPool::Slot object_slot_ = {};
  int32_t anim_index_ = 0;

  RefPtr<Bitmap> tileset_;
  Autotile autotiles_[7] = {};
  RefPtr<Table> map_data_;
  RefPtr<Table> flash_data_;
  RefPtr<Table> priorities_;
  int32_t ox_ = 0, oy_ = 0;
  bool xrepeat_ = true, yrepeat_ = true;
  int32_t tilesize_ = 32;

  TileLayer ground_layer_;
  std::vector<TileLayer> above_layers_;
};

}  // namespace urge
