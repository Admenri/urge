// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/common/exception.h"
#include "content/content_config.h"
#include "content/gpu/gpu_device.h"
#include "content/gpu/gpu_resource.h"
#include "content/scene/camera.h"
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
  RenderContext(World* world,
                scoped_refptr<GPUTextureView> rtv,
                scoped_refptr<GPUTextureView> dsv);

  RenderContext(const RenderContext&) = delete;
  RenderContext& operator=(const RenderContext&) = delete;

 public:
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
  World* world_;

  scoped_refptr<GPUTextureView> render_target_view_;
  scoped_refptr<GPUTextureView> depth_stencil_view_;
};

///
/// Viewport
///

URGE_BINDING()
class Viewport : public Object {
 public:
  Viewport();

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

 public:
  URGE_BINDING()
  using RenderCallback =
      base::RepeatingCallback<void(scoped_refptr<RenderContext> context,
                                   earray<scoped_refptr<Camera>> cameras)>;

  URGE_BINDING()
  static scoped_refptr<Viewport> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(World, scoped_refptr<World>);

  URGE_BINDING()
  void SetupRenderProcess(RenderCallback callback, URGE_EXCEPTION);

  URGE_BINDING()
  void Render(scoped_refptr<GPUTextureView> render_target,
              scoped_refptr<GPUTextureView> depth_stencil,
              URGE_EXCEPTION);

 private:
  scoped_refptr<World> world_;
  RenderCallback render_process_;
};

}  // namespace content
