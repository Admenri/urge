// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_TILEMAP2_IMPL_H_
#define CONTENT_RENDER_TILEMAP2_IMPL_H_

#include <array>
#include <list>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/canvas/canvas_impl.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/common/table_impl.h"
#include "content/common/tone_impl.h"
#include "content/context/disposable.h"
#include "content/public/engine_tilemap2.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/device/render_device.h"

namespace content {

class Tilemap2Impl;

class TilemapBitmapImpl : public TilemapBitmap {
 public:
  TilemapBitmapImpl(base::WeakPtr<Tilemap2Impl> tilemap);
  ~TilemapBitmapImpl() override;

  TilemapBitmapImpl(const TilemapBitmapImpl&) = delete;
  TilemapBitmapImpl& operator=(const TilemapBitmapImpl&) = delete;

  scoped_refptr<Bitmap> Get(int32_t index,
                            ExceptionState& exception_state) override;
  void Put(int32_t index,
           scoped_refptr<Bitmap> texture,
           ExceptionState& exception_state) override;

 private:
  base::WeakPtr<Tilemap2Impl> tilemap_;
};

class Tilemap2Impl : public Tilemap2, public EngineObject, public Disposable {
 public:
  struct Agent {
    renderer::QuadBatch batch;
    renderer::Binding_Tilemap2 shader_binding;

    int32_t ground_draw_count = 0;
    int32_t above_draw_count = 0;

    RRefPtr<Diligent::ITexture> atlas_texture;
    RRefPtr<Diligent::ITextureView> atlas_binding;
    RRefPtr<Diligent::IBuffer> uniform_buffer;
  };

  enum BitmapID {
    TILE_A1 = 0,
    TILE_A2,
    TILE_A3,
    TILE_A4,
    TILE_A5,

    TILE_B,
    TILE_C,
    TILE_D,
    TILE_E,
  };

  struct AtlasBlock {
    BitmapID tile_id;
    base::Rect src_rect;
    base::Vec2i dest_pos;
  };

  struct AtlasCompositeCommand {
    BitmapAgent* texture;
    base::Rect src_rect;
    base::Vec2i dst_pos;
  };

  Tilemap2Impl(ExecutionContext* execution_context,
               scoped_refptr<ViewportImpl> parent,
               int32_t tilesize);
  ~Tilemap2Impl() override;

  Tilemap2Impl(const Tilemap2Impl&) = delete;
  Tilemap2Impl& operator=(const Tilemap2Impl&) = delete;

  void SetLabel(const std::string& label,
                ExceptionState& exception_state) override;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void Update(ExceptionState& exception_state) override;
  scoped_refptr<TilemapBitmap> Bitmaps(
      ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(MapData, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(FlashData, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Passages, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Flags, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(RepeatX, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(RepeatY, bool);

 private:
  friend class TilemapBitmapImpl;
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "Tilemap2"; }
  void GroundNodeHandlerInternal(DrawableNode::RenderStage stage,
                                 DrawableNode::RenderControllerParams* params);
  void AboveNodeHandlerInternal(DrawableNode::RenderStage stage,
                                DrawableNode::RenderControllerParams* params);

  void UpdateViewportInternal(const base::Rect& viewport,
                              const base::Vec2i& viewport_origin);

  base::Vec2i MakeAtlasInternal(std::vector<AtlasCompositeCommand>& commands);
  void ParseMapDataInternal(std::vector<renderer::Quad>& ground_cache,
                            std::vector<renderer::Quad>& above_cache);

  void AtlasModifyHandlerInternal();
  void MapDataModifyHandlerInternal();

  void GPUCreateTilemapInternal();
  void GPUMakeAtlasInternal(
      Diligent::IDeviceContext* render_context,
      int32_t tilesize,
      const base::Vec2i& atlas_size,
      std::vector<Tilemap2Impl::AtlasCompositeCommand> make_commands);
  void GPUUpdateQuadBatchInternal(Diligent::IDeviceContext* render_context,
                                  std::vector<renderer::Quad> ground_cache,
                                  std::vector<renderer::Quad> above_cache);
  void GPUUpdateTilemapUniformInternal(Diligent::IDeviceContext* render_context,
                                       const base::Vec2& offset,
                                       const base::Vec2& anim_offset,
                                       int32_t tilesize);
  void GPURenderGroundLayerInternal(Diligent::IDeviceContext* render_context,
                                    Diligent::IBuffer* world_binding);
  void GPURenderAboveLayerInternal(Diligent::IDeviceContext* render_context,
                                   Diligent::IBuffer* world_binding);

  struct BitmapInfo {
    scoped_refptr<CanvasImpl> bitmap;
    base::CallbackListSubscription observer;
  };

  DrawableNode ground_node_;
  DrawableNode above_node_;
  Agent agent_;
  int32_t tilesize_ = 32;
  base::Rect render_viewport_;
  base::Vec2i render_offset_;
  base::Vec2 animation_offset_;
  int32_t flash_count_ = 0;
  int32_t flash_timer_ = 0;
  int32_t flash_opacity_ = 0;
  int32_t frame_index_ = 0;
  bool atlas_dirty_ = false;
  bool map_buffer_dirty_ = false;
  bool enable_shadow_layer_ = false;

  std::array<BitmapInfo, 9> bitmaps_;
  scoped_refptr<TableImpl> map_data_;
  scoped_refptr<TableImpl> flash_data_;
  scoped_refptr<TableImpl> flags_;
  scoped_refptr<ViewportImpl> viewport_;
  base::Vec2i origin_;
  base::Vec2i repeat_;

  base::CallbackListSubscription map_data_observer_;
  base::CallbackListSubscription flash_data_observer_;
  base::CallbackListSubscription flags_observer_;

  base::WeakPtrFactory<Tilemap2Impl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  //! CONTENT_RENDER_TILEMAP2_IMPL_H_
