// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_TILEMAP_IMPL_H_
#define CONTENT_RENDER_TILEMAP_IMPL_H_

#include <array>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/canvas/canvas_impl.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/common/table_impl.h"
#include "content/common/tone_impl.h"
#include "content/context/disposable.h"
#include "content/public/engine_tilemap.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/device/render_device.h"

namespace content {

/*
  The Z-coordinate of each sprite used to create a tilemap is fixed at a
    specific value:

    1. Tiles with a priority of 0 always have a Z-coordinate of 0.
    2. Priority 1 tiles placed at the top edge of the screen have a
      Z-coordinate of 64.
    3. Every time the priority increases by 1 or the next tile down is selected,
      the Z-coordinate increases by 32.
    4. The Z-coordinate changes accordingly as the tilemap scrolls vertically.
*/

class TilemapImpl;

class TilemapAutotileImpl : public TilemapAutotile {
 public:
  TilemapAutotileImpl(base::WeakPtr<TilemapImpl> tilemap);
  ~TilemapAutotileImpl() override;

  TilemapAutotileImpl(const TilemapAutotileImpl&) = delete;
  TilemapAutotileImpl& operator=(const TilemapAutotileImpl&) = delete;

  scoped_refptr<Bitmap> Get(int32_t index,
                            ExceptionState& exception_state) override;
  void Put(int32_t index,
           scoped_refptr<Bitmap> texture,
           ExceptionState& exception_state) override;

 private:
  base::WeakPtr<TilemapImpl> tilemap_;
};

class TilemapImpl : public Tilemap, public EngineObject, public Disposable {
 public:
  struct GPUData {
    renderer::QuadBatch batch;
    renderer::Binding_Tilemap shader_binding;

    int32_t ground_draw_count;
    std::vector<int32_t> above_draw_count;

    RRefPtr<Diligent::ITexture> atlas_texture;
    RRefPtr<Diligent::ITextureView> atlas_binding;
    RRefPtr<Diligent::IBuffer> uniform_buffer;
  };

  struct AtlasCompositeCommand {
    BitmapTexture* texture;
    base::Rect src_rect;
    base::Vec2i dst_pos;
  };

  enum class AutotileType {
    COMMON = 0,
    SINGLE,
  };

  TilemapImpl(ExecutionContext* execution_context,
              scoped_refptr<ViewportImpl> parent,
              int32_t tilesize);
  ~TilemapImpl() override;

  TilemapImpl(const TilemapImpl&) = delete;
  TilemapImpl& operator=(const TilemapImpl&) = delete;

  URGE_DECLARE_DISPOSABLE;
  void Update(ExceptionState& exception_state) override;
  scoped_refptr<TilemapAutotile> Autotiles(
      ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Tileset, scoped_refptr<Bitmap>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(MapData, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(FlashData, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Priorities, scoped_refptr<Table>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(RepeatX, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(RepeatY, bool);

 private:
  friend class TilemapAutotileImpl;
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "Tilemap"; }
  void GroundNodeHandlerInternal(DrawableNode::RenderStage stage,
                                 DrawableNode::RenderControllerParams* params);
  void AboveNodeHandlerInternal(int32_t layer_index,
                                DrawableNode::RenderStage stage,
                                DrawableNode::RenderControllerParams* params);

  base::Vec2i MakeAtlasInternal(std::vector<AtlasCompositeCommand>& commands);
  void UpdateViewportInternal(const base::Rect& viewport,
                              const base::Vec2i& viewport_origin);
  void ParseMapDataInternal(
      std::vector<renderer::Quad>& ground_cache,
      std::vector<std::vector<renderer::Quad>>& aboves_cache);

  void SetupTilemapLayersInternal(const base::Rect& viewport);
  void ResetAboveLayersOrderInternal();

  void AtlasModifyHandlerInternal();
  void MapDataModifyHandlerInternal();

  void GPUCreateTilemapInternal();
  void GPUMakeAtlasInternal(
      Diligent::IDeviceContext* render_context,
      const base::Vec2i& atlas_size,
      std::vector<TilemapImpl::AtlasCompositeCommand> make_commands);
  void GPUUploadTilesBatchInternal(
      Diligent::IDeviceContext* render_context,
      std::vector<renderer::Quad> ground_cache,
      std::vector<std::vector<renderer::Quad>> aboves_cache);
  void GPUUpdateTilemapUniformInternal(Diligent::IDeviceContext* render_context,
                                       const base::Vec2& offset,
                                       int32_t tilesize,
                                       int32_t anim_index);
  void GPURenderGroundLayerInternal(Diligent::IDeviceContext* render_context,
                                    Diligent::IBuffer* world_binding);
  void GPURenderAboveLayerInternal(Diligent::IDeviceContext* render_context,
                                   Diligent::IBuffer* world_binding,
                                   int32_t index);

  struct AutotileInfo {
    scoped_refptr<CanvasImpl> bitmap;
    AutotileType type = AutotileType::COMMON;
    int32_t animation_frame_count;
    base::CallbackListSubscription observer;
  };

  GPUData gpu_;
  DrawableNode ground_node_;
  std::vector<std::unique_ptr<DrawableNode>> above_nodes_;

  int32_t tilesize_ = 32;
  int32_t max_atlas_size_ = 0;
  int32_t max_vertical_count_ = 0;
  base::Rect last_viewport_;
  base::Rect render_viewport_;
  base::Vec2i render_offset_;
  int32_t max_anim_index_ = 1;
  int32_t max_autotile_frame_count_ = 1;
  int32_t anim_index_ = 0;
  int32_t flash_timer_ = 0;
  int32_t flash_opacity_ = 0;
  bool atlas_dirty_ = false;
  bool map_buffer_dirty_ = false;

  base::CallbackListSubscription tileset_observer_;
  base::CallbackListSubscription map_data_observer_;
  base::CallbackListSubscription flash_data_observer_;
  base::CallbackListSubscription priorities_observer_;

  scoped_refptr<ViewportImpl> viewport_;
  scoped_refptr<CanvasImpl> tileset_;
  scoped_refptr<TableImpl> map_data_;
  scoped_refptr<TableImpl> flash_data_;
  scoped_refptr<TableImpl> priorities_;
  std::array<AutotileInfo, 7> autotiles_;
  base::Vec2i origin_;
  base::Vec2i repeat_;

  base::WeakPtrFactory<TilemapImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  //! CONTENT_RENDER_TILEMAP_IMPL_H_
