// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPINE_SKELETON_RENDERER_H_
#define COMPONENTS_SPINE_SKELETON_RENDERER_H_

#include "spine/spine.h"

#include "base/memory/allocator.h"
#include "base/worker/thread_worker.h"
#include "components/filesystem/io_service.h"
#include "renderer/device/render_device.h"
#include "renderer/pipeline/render_pipeline.h"

namespace spine {

using SpineVertexBatch = renderer::BatchBuffer<renderer::SpineVertex,
                                               Diligent::BIND_VERTEX_BUFFER,
                                               Diligent::BUFFER_MODE_UNDEFINED,
                                               Diligent::CPU_ACCESS_WRITE,
                                               Diligent::USAGE_DYNAMIC>;

class DiligentTextureLoader : public TextureLoader {
 public:
  DiligentTextureLoader(renderer::RenderDevice* device,
                        filesystem::IOService* io_service);
  ~DiligentTextureLoader() override;

  DiligentTextureLoader(const DiligentTextureLoader&) = delete;
  DiligentTextureLoader& operator=(const DiligentTextureLoader&) = delete;

  void load(AtlasPage& page, const String& path) override;
  void unload(void* texture) override;

 private:
  void* GPUCreateTextureInternal(SDL_Surface* atlas_surface,
                                 TEXTURE_FILTER_ENUM min_filter,
                                 TEXTURE_FILTER_ENUM mag_filter,
                                 TextureWrap uwrap,
                                 TextureWrap vwrap);

  renderer::RenderDevice* device_;
  filesystem::IOService* io_service_;
};

class DiligentRenderer {
 public:
  DiligentRenderer(renderer::RenderDevice* device);
  ~DiligentRenderer();

  DiligentRenderer(const DiligentRenderer&) = delete;
  DiligentRenderer& operator=(const DiligentRenderer&) = delete;

  void Update(Diligent::IDeviceContext* context, spine::Skeleton* skeleton);
  void Render(Diligent::IDeviceContext* context,
              Diligent::IBuffer* world_buffer,
              bool premultiplied_alpha);

 private:
  void GPURenderSkeletonCommandsInternal(
      Diligent::IDeviceContext* render_context,
      Diligent::ITexture* atlas,
      BlendMode blend_mode,
      bool premultiplied_alpha,
      Diligent::IBuffer* world_buffer,
      uint32_t num_vertices,
      uint32_t vertices_offset);

  renderer::RenderDevice* device_;
  SpineVertexBatch vertex_batch_;
  renderer::Binding_Base shader_binding_;

  base::Vector<renderer::SpineVertex> vertex_cache_;
  base::OwnedPtr<SkeletonRenderer> skeleton_renderer_;
  RenderCommand* pending_commands_;
};

}  // namespace spine

#endif  //! COMPONENTS_SPINE_SKELETON_RENDERER_H_
