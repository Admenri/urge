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
  struct Agent {
    renderer::QuadBatch batch;
    renderer::Binding_Tilemap shader_binding;

    int32_t ground_draw_count;
    base::Vector<int32_t> above_draw_count;

    RRefPtr<Diligent::ITexture> atlas_texture;
    RRefPtr<Diligent::ITextureView> atlas_binding;
    RRefPtr<Diligent::IBuffer> uniform_buffer;
  };

  struct AtlasCompositeCommand {
    BitmapAgent* texture;
    base::Rect src_rect;
    base::Vec2i dst_pos;
  };

  enum class AutotileType {
    ANIMATED = 0,
    STATIC,
    SINGLE_ANIMATED,
  };

  TilemapImpl(ExecutionContext* execution_context,
              scoped_refptr<ViewportImpl> parent,
              int32_t tilesize);
  ~TilemapImpl() override;

  TilemapImpl(const TilemapImpl&) = delete;
  TilemapImpl& operator=(const TilemapImpl&) = delete;

  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
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

 private:
  friend class TilemapAutotileImpl;
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "Tilemap"; }
  void GroundNodeHandlerInternal(DrawableNode::RenderStage stage,
                                 DrawableNode::RenderControllerParams* params);
  void AboveNodeHandlerInternal(int32_t layer_index,
                                DrawableNode::RenderStage stage,
                                DrawableNode::RenderControllerParams* params);

  base::Vec2i MakeAtlasInternal(base::Vector<AtlasCompositeCommand>& commands);
  void UpdateViewportInternal(const base::Rect& viewport,
                              const base::Vec2i& viewport_origin);
  void ParseMapDataInternal(
      base::Vector<renderer::Quad>& ground_cache,
      base::Vector<base::Vector<renderer::Quad>>& aboves_cache);

  void SetupTilemapLayersInternal(const base::Rect& viewport);
  void ResetAboveLayersOrderInternal();

  void AtlasModifyHandlerInternal();
  void MapDataModifyHandlerInternal();

  void GPUCreateTilemapInternal();
  void GPUMakeAtlasInternal(
      Diligent::IDeviceContext* render_context,
      const base::Vec2i& atlas_size,
      base::Vector<TilemapImpl::AtlasCompositeCommand> make_commands);
  void GPUUploadTilesBatchInternal(
      Diligent::IDeviceContext* render_context,
      base::Vector<renderer::Quad> ground_cache,
      base::Vector<base::Vector<renderer::Quad>> aboves_cache);
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
    AutotileType type = AutotileType::ANIMATED;
    base::CallbackListSubscription observer;
  };

  DrawableNode ground_node_;
  base::Vector<base::OwnedPtr<DrawableNode>> above_nodes_;
  Agent agent_;
  int32_t tilesize_ = 32;
  int32_t max_atlas_size_ = 0;
  int32_t max_vertical_count_ = 0;
  base::Rect render_viewport_;
  base::Vec2i render_offset_;
  bool atlas_dirty_ = false;
  bool map_buffer_dirty_ = false;
  base::Rect last_viewport_;
  int32_t anim_index_ = 0;
  int32_t flash_count_ = 0;
  int32_t flash_timer_ = 0;
  int32_t flash_opacity_ = 0;

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

  base::WeakPtrFactory<TilemapImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  //! CONTENT_RENDER_TILEMAP_IMPL_H_
