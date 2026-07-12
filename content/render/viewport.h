// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/content_config.h"
#include "content/gpu/gpu_device.h"
#include "content/gpu/gpu_resource.h"
#include "content/render/camera.h"
#include "content/scene/world.h"

namespace content {

URGE_BINDING()
class SortingSettings : public Object {
 public:
  URGE_BINDING()
  enum class SortingCriteria : uint64_t {
    OrderSorting = 1 << 0,
    RenderQueue = 1 << 1,
    BackToFront = 1 << 2,
    FrontToBack = 1 << 3,
  };

  URGE_BINDING()
  scoped_refptr<Vector3d> cameraPosition;

  URGE_BINDING()
  SortingCriteria criteria;
};

URGE_BINDING()
class DrawingSettings : public Object {
 public:
  URGE_BINDING()
  estring passTag = {};

  URGE_BINDING()
  estring passName = {};

  URGE_BINDING()
  scoped_refptr<SortingSettings> sortingSettings = nullptr;
};

URGE_BINDING()
class FilteringSettings : public Object {
 public:
  URGE_BINDING()
  uint64_t cullingMask = 0xFFFFFFFFFFFFFFFFu;

  URGE_BINDING()
  uint32_t minRenderQueue = 0x00000000u;

  URGE_BINDING()
  uint32_t maxRenderQueue = 0xFFFFFFFFu;
};

URGE_BINDING()
class CullingResults : public Object {
 public:
  CullingResults();

  CullingResults(const CullingResults&) = delete;
  CullingResults& operator=(const CullingResults&) = delete;

 public:
  URGE_BINDING()
  uint32_t GetVisibleObjectCount(URGE_EXCEPTION);

 private:
};

///
/// RenderContext
///

URGE_BINDING()
class RenderContext : public Object {
 public:
  RenderContext();

  RenderContext(const RenderContext&) = delete;
  RenderContext& operator=(const RenderContext&) = delete;

 public:
  URGE_BINDING()
  scoped_refptr<GPUDevice> GetDevice(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUQueue> GetQueue(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUTextureView> GetRenderTargetView(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUTextureView> GetDepthStencilView(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<CullingResults> Cull(scoped_refptr<Camera> camera,
                                     URGE_EXCEPTION);

  URGE_BINDING()
  void DrawRenderers(scoped_refptr<GPURenderPassEncoder> pass,
                     scoped_refptr<CullingResults> culling_results,
                     scoped_refptr<DrawingSettings> drawing_settings,
                     scoped_refptr<FilteringSettings> filtering_settings,
                     URGE_EXCEPTION);

 private:
};

///
/// RendererProcess
///

URGE_BINDING()
class RendererProcess : public Object {
 public:
  RendererProcess();

  RendererProcess(const RendererProcess&) = delete;
  RendererProcess& operator=(const RendererProcess&) = delete;

 public:
  URGE_BINDING()
  using RenderCallback =
      base::RepeatingCallback<void(scoped_refptr<RenderContext> context,
                                   earray<scoped_refptr<Camera>> cameras)>;

  URGE_BINDING()
  static scoped_refptr<RendererProcess> New(RenderCallback callback,
                                            URGE_EXCEPTION);

 private:
  RenderCallback on_render_;
};

///
/// Viewport
///

URGE_BINDING()
class Viewport : public Object {
 public:
  Viewport(const glm::ivec2& size);

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<Viewport> New(scoped_refptr<Vector2i> size,
                                     URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(World, scoped_refptr<World>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Renderer, scoped_refptr<RendererProcess>);

  URGE_BINDING()
  scoped_refptr<Vector2i> GetSize(URGE_EXCEPTION);

 private:
  glm::ivec2 size_;
  scoped_refptr<World> world_;
  scoped_refptr<RendererProcess> process_;
};

}  // namespace content
