// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TILEMAP_H_
#define CONTENT_PUBLIC_TILEMAP_H_

#include "base/memory/ref_counted.h"
#include "content/common/tileutils.h"
#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/table.h"
#include "content/public/viewport.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"

#include <array>

namespace content {

class TilemapGroundLayer;
class TilemapZLayer;

class Tilemap : public base::RefCounted<Tilemap>,
                public GraphicsElement,
                public Disposable {
 public:
  Tilemap(scoped_refptr<Graphics> screen,
          scoped_refptr<Viewport> viewport = nullptr,
          int tilesize = 32);
  ~Tilemap();

  Tilemap(const Tilemap&) = delete;
  Tilemap& operator=(const Tilemap&) = delete;

  void Update();
  scoped_refptr<Viewport> GetViewport() const;

  scoped_refptr<Bitmap> GetTileset() const;
  void SetTileset(scoped_refptr<Bitmap> bitmap);

  scoped_refptr<Bitmap> GetAutotiles(int index) const;
  void SetAutotiles(int index, scoped_refptr<Bitmap> bitmap);

  scoped_refptr<Table> GetMapData() const;
  void SetMapData(scoped_refptr<Table> map_data);

  scoped_refptr<Table> GetFlashData() const;
  void SetFlashData(scoped_refptr<Table> flash_data);

  scoped_refptr<Table> GetPriorities() const;
  void SetPriorities(scoped_refptr<Table> flags);

  bool GetVisible() const;
  void SetVisible(bool visible);

  int GetOX() const;
  void SetOX(int ox);

  int GetOY() const;
  void SetOY(int oy);

 private:
  friend class TilemapGroundLayer;
  friend class TilemapZLayer;
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Tilemap"; }

  void InitTilemapData();
  void BeforeTilemapComposite();

  void MakeAtlasInternal();
  void UpdateViewportInternal(
      const DrawableParent::ViewportInfo& viewport_rect);
  void UpdateMapBufferInternal();
  void RaiseUpdateAtlasInternal();
  void RaiseUpdateBufferInternal();
  void SetupDrawLayersInternal();
  void ResetZLayerInternal();
  void DrawFlashLayerInternal(CompositeTargetInfo* target_info);
  void DrawGroundLayerInternal(CompositeTargetInfo* target_info);
  void DrawAboveLayerInternal(int index, CompositeTargetInfo* target_info);

  enum class AutotileType {
    Animated = 0,
    Static,
    SingleAnimated,
  };

  struct AutotileInfo {
    scoped_refptr<Bitmap> bitmap;
    AutotileType type;
  };

  scoped_refptr<Bitmap> tileset_;
  std::array<AutotileInfo, 7> autotiles_;

  scoped_refptr<Table> map_data_;
  scoped_refptr<Table> flash_data_;
  scoped_refptr<Table> priorities_;

  scoped_refptr<Viewport> viewport_;
  bool visible_ = true;
  base::Vec2i origin_;
  int tile_size_;

  std::unique_ptr<renderer::QuadArray> tilemap_quads_;
  std::unique_ptr<TilemapFlashLayer> flash_layer_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> atlas_texture_;

  std::vector<renderer::GeometryVertexLayout::Data> ground_vertices_;
  std::vector<std::vector<renderer::GeometryVertexLayout::Data>>
      above_vertices_;
  std::vector<size_t> above_offsets_;

  std::unique_ptr<TilemapGroundLayer> ground_layer_;
  std::vector<std::unique_ptr<TilemapZLayer>> above_layers_;

  base::Vec2i tilemap_offset_;
  base::Rect tilemap_viewport_;

  bool atlas_need_update_ = false;
  bool map_buffer_need_update_ = false;

  int frame_index_ = 0;
  int animation_index_ = 0;
  int flash_alpha_index_ = 0;

  base::CallbackListSubscription map_data_observer_;
  base::CallbackListSubscription priorities_observer_;
  base::CallbackListSubscription bitmap_observers_[8];
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_TILEMAP_H_
