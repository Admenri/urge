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

//! One above layer of a TilemapXP; its Z follows from the priority and the row
//! of its tiles, see TilemapXP::UpdateOrder().
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

//! The tilemap of RGSS1: draws a Table of tile ids out of seven animated autotiles
//! and one tileset, split by priority into the ground layer and up to five above
//! layers, inside the viewport it is given.
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

  //! The number of above layers the priority of a tile can put it in.
  static constexpr int32_t kMaxPriorities = 5;

  //! One tile of the map: the rectangle it reads from its bitmap and the
  //! rectangle it is drawn to, both in pixels of the render target.
  struct TileQuad {
    RefPtr<Bitmap> texture;
    RectF source;
    RectF destination;
  };

  //! One draw of a layer: the bitmap its tiles read from and the range the
  //! tiles occupy in the vertex batch which was uploaded for the layer.
  struct TileDraw {
    RefPtr<Bitmap> texture;
    PrimitiveEmitter::Slot slot;
  };

  //! The geometry of one layer: the emitter which uploaded it and the draws it
  //! is read with.
  struct TileLayer {
    PrimitiveEmitter primitive;
    std::vector<TileDraw> draws;
    bool valid = false;
  };

  //! An autotile of the tilemap: its bitmap and the number of frames the
  //! animation of it holds.
  struct Autotile {
    RefPtr<Bitmap> texture;
    int32_t frames = 0;
  };

  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  //! Reads the rect and the origin of the viewport of this tilemap, or the size
  //! of the screen when it has none, into render_viewport_ and render_offset_.
  void UpdateViewport();
  //! Makes the above layers match the height of the region this frame draws,
  //! see the comment of UpdateAboves().
  void UpdateAboves();
  //! Sets the Z of every above layer from the row of the screen it stands for,
  //! see the comment of UpdateOrder().
  void UpdateOrder();
  //! Collects the tiles of the region of this frame into \p ground and \p
  //! aboves, the latter one entry per above layer.
  void ParseTiles(std::vector<TileQuad>* ground,
                  std::vector<std::vector<TileQuad>>* aboves);
  //! Collects, emits and uploads the ground layer and every above layer.
  void BuildLayers();
  //! Binds the pipeline and the object set of a layer and issues the draws of
  //! it against the vertex buffer the layer uploaded.
  void DrawLayer(DrawParam param, const TileLayer& layer);
  //! The drawing stage of the child above of id, see TilemapXPAbove.
  void DrawAboveLayer(DrawParam param, int32_t id);
  //! True when the above layer of id holds a tile this frame, i.e. when its
  //! child has something to draw.
  bool HasAboveLayer(int32_t id) const;

  std::vector<RefPtr<TilemapXPAbove>> aboves_;
  //! The region of the map this frame draws, in tiles.
  RectI render_viewport_ = {};
  //! The offset the tiles of this frame are placed at, in pixels.
  glm::vec2 render_offset_ = glm::vec2(0.0f);
  //! The slot of the object pool this frame put the identity transform of this
  //! tilemap in, see BuildLayers().
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

  //! The ground layer of this frame and its above layers, built by Prepare().
  TileLayer ground_layer_;
  std::vector<TileLayer> above_layers_;
};

}  // namespace urge
