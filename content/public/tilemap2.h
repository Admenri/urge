// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TILEMAP2_H_
#define CONTENT_PUBLIC_TILEMAP2_H_

#include <array>

#include "base/memory/ref_counted.h"
#include "content/common/tileutils.h"
#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/table.h"
#include "content/public/viewport.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"

namespace content {

class TilemapGroundLayer2;
class TilemapAboveLayer2;

// Reference: https://www.tktkgame.com/tkool/memo/vx/tile_id.html
class Tilemap2 : public base::RefCounted<Tilemap2>,
                 public GraphicsElement,
                 public Disposable {
 public:
  using TilemapBitmapID = enum {
    TileA1 = 0,
    TileA2,
    TileA3,
    TileA4,
    TileA5,

    TileB,
    TileC,
    TileD,
    TileE,
  };

  Tilemap2(scoped_refptr<Graphics> screen,
           scoped_refptr<Viewport> viewport = nullptr,
           int tilesize = 32);
  ~Tilemap2() override;

  Tilemap2(const Tilemap2&) = delete;
  Tilemap2& operator=(const Tilemap2&) = delete;

  void Update();

  scoped_refptr<Bitmap> GetBitmap(int index) const;
  void SetBitmap(int index, scoped_refptr<Bitmap> bitmap);

  scoped_refptr<Table> GetMapData() const;
  void SetMapData(scoped_refptr<Table> map_data);

  scoped_refptr<Table> GetFlashData() const;
  void SetFlashData(scoped_refptr<Table> flash_data);

  scoped_refptr<Table> GetFlags() const;
  void SetFlags(scoped_refptr<Table> flags);

  scoped_refptr<Viewport> GetViewport() const;
  void SetViewport(scoped_refptr<Viewport> viewport);

  bool GetVisible() const;
  void SetVisible(bool visible);

  int GetOX() const;
  void SetOX(int ox);

  int GetOY() const;
  void SetOY(int oy);

 private:
  friend class TilemapGroundLayer2;
  friend class TilemapAboveLayer2;

  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Tilemap2"; }

  void InitDrawableData();
  void BeforeTilemapComposite();

  void CreateTileAtlasInternal();
  void UpdateTilemapViewportInternal();
  void ParseMapDataBufferInternal();
  void SetAtlasUpdateInternal();
  void UpdateMapBufferInternal();
  void DrawFlashLayerInternal(CompositeTargetInfo* target_info);
  void DrawGroundLayerInternal(CompositeTargetInfo* target_info);
  void DrawAboveLayerInternal(CompositeTargetInfo* target_info);

  std::unique_ptr<TilemapGroundLayer2> ground_;
  std::unique_ptr<TilemapAboveLayer2> above_;

  scoped_refptr<Viewport> viewport_;
  bool visible_ = true;
  base::Vec2i origin_;

  std::array<scoped_refptr<Bitmap>, 9> bitmaps_;

  scoped_refptr<Table> map_data_;
  scoped_refptr<Table> flash_data_;
  scoped_refptr<Table> flags_;

  std::unique_ptr<renderer::QuadArray> tilemap_quads_;

  std::vector<renderer::GeometryVertexLayout::Data> ground_vertices_;
  std::vector<renderer::GeometryVertexLayout::Data> above_vertices_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> atlas_texture_;

  int tile_size_;
  base::Rect tilemap_viewport_;
  base::Vec2i tilemap_offset_;

  bool atlas_need_update_ = false;
  bool buffer_need_update_ = false;

  std::unique_ptr<TilemapFlashLayer> flash_layer_;
  int frame_index_ = 0;
  int flash_alpha_index_ = 0;
  base::Vec2 animation_offset_;

  base::CallbackListSubscription map_data_observer_;
  base::CallbackListSubscription flags_observer_;
  base::CallbackListSubscription bitmap_observers_[9];

  base::WeakPtrFactory<Tilemap2> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_TILEMAP2_H_