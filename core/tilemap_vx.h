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

class TilemapVX;

/*! The layer of a TilemapVX which holds the tiles drawn over the player. RGSS3
    splits the O(2) region between the map and this layer, so the tiles of the
    map whose flag carries the 0x10 bit are drawn by a child node whose Z is 200
    above the tilemap, which is what puts an object standing on such a tile
    between the two layers.

    The node draws nothing of its own: the tilemap collects and emits the tiles
    of both of its layers once per frame, this node only triggers the draw of
    the upper one when its turn in the render order arrives, see
    TilemapVX::Prepare() and TilemapVX::DrawAboveLayer(). */
class TilemapVXAbove : public Node {
 public:
  TilemapVXAbove(TilemapVX* parent, RefPtr<Viewport> viewport);

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  TilemapVX* parent_ = nullptr;
};

/*! The tilemap of RGSS2/RGSS3: the drawing of a Table of tile ids out of five
    autotile bitmaps and one tileset.

    The class is a Node and no Viewport of its own: RGSS places a tilemap inside
    the viewport it was given and paints the region of that viewport, so the
    class reads the rect and the origin of the viewport to know what to draw and
    leaves the clipping of it to the viewport, see UpdateViewport(). A tilemap
    without a viewport covers the whole screen.

    A tilemap does not take part in the transform of the node hierarchy: the
    tiles of a map sit on the render target at whole pixels, so a position or a
    scale of the tilemap would only move them off the grid. The object transform
    every tile is drawn with is therefore the identity and the tiles place
    themselves by their vertex position, the same way a Plane places its tiles,
    see BuildLayer(). */
class TilemapVX : public Node {
 public:
  /*-export.begin-*/
  TilemapVX(RefPtr<Viewport> viewport = nullptr);
  ~TilemapVX() override;

  void Update();

  void SetBitmap(int32_t index, RefPtr<Bitmap> bitmap);
  RefPtr<Bitmap> GetBitmap(int32_t index);

  ATTR(RefPtr<Viewport>, Viewport);
  ATTR(bool, Visible) override;
  ATTR(int32_t, Z) override;
  ATTR(RefPtr<Table>, MapData);
  ATTR(RefPtr<Table>, FlashData);
  ATTR(RefPtr<Table>, Flags);
  ATTR(RefPtr<Table>, Passages);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  /*-export.end-*/

 private:
  friend class TilemapVXAbove;

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

  //! Builds the shadow bitmap the shadow tiles of the map are read from, see
  //! process_shadow_tile().
  void CreateShadowSet();
  //! Reads the rect and the origin of the viewport of this tilemap, or the size
  //! of the screen when it has none, into render_viewport_ and render_offset_.
  void UpdateViewport();
  //! Collects the tiles of one half of the map into \p quads: the tiles of the
  //! map layer when \p above is false, the ones of the layer over the player
  //! when it is true.
  void CollectMapData(bool above, std::vector<TileQuad>* quads);
  //! Collects, emits and uploads one layer of the map into \p layer.
  void BuildLayer(bool above, TileLayer* layer);
  //! Binds the pipeline and the object set of a layer and issues the draws of
  //! it against the vertex buffer the layer uploaded.
  void DrawLayer(DrawParam param, TileLayer* layer);
  //! The drawing stage of the child above, see TilemapVXAbove.
  void DrawAboveLayer(DrawParam param);
  //! True when the layer over the player holds a tile this frame, i.e. when the
  //! child above has something to draw.
  bool HasAboveLayer() const { return above_layer_.valid; }

  bool rgss3_style_ = true;

  RefPtr<TilemapVXAbove> above_;
  RefPtr<Bitmap> shadow_texture_;
  //! The region of the map this frame draws, in tiles.
  RectI render_viewport_ = {};
  //! The offset the tiles of this frame are placed at, in pixels.
  glm::vec2 render_offset_ = glm::vec2(0.0f);
  //! The slot of the object pool this frame put the identity transform of this
  //! tilemap in, see BuildLayer().
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

  //! The two layers of the map, the map itself and the one drawn over the
  //! player, both built by Prepare().
  TileLayer map_layer_;
  TileLayer above_layer_;
};

}  // namespace urge
