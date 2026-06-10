// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/content_config.h"
#include "content/gpu/gpu_device.h"
#include "content/gpu/gpu_resource.h"
#include "content/render/camera.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
struct SortingSettings : public Object {
  scoped_refptr<Vector3d> cameraPosition;
  SortingCriteria criteria;

  static estruct<SortingSettings> New(scoped_refptr<Camera>, URGE_EXCEPTION);
};

URGE_BINDING()
struct DrawingSettings : public Object {
  estruct<SortingSettings> sortingSettings;
};

URGE_BINDING()
struct FilteringSettings : public Object {
  uint64_t cullingMask = std::numeric_limits<uint64_t>::max();
  uint32_t minRenderQueue = std::numeric_limits<uint32_t>::min();
  uint32_t maxRenderQueue = std::numeric_limits<uint32_t>::max();
};

URGE_BINDING()
struct CullingSettings : public Object {
  uint64_t cullingMask = std::numeric_limits<uint64_t>::max();

  static estruct<CullingSettings> New(scoped_refptr<Camera>, URGE_EXCEPTION);
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
  scoped_refptr<CullingResults> Cull(estruct<CullingSettings> settings,
                                     URGE_EXCEPTION);

  URGE_BINDING()
  void DrawRenderers(scoped_refptr<GPURenderPassEncoder> pass,
                     scoped_refptr<CullingResults> culling_results,
                     estruct<DrawingSettings> drawing_settings,
                     estruct<FilteringSettings> filtering_settings,
                     URGE_EXCEPTION);

 private:
};

URGE_BINDING()
class RenderProcess : public Object {
 public:
  RenderProcess();

  RenderProcess(const RenderProcess&) = delete;
  RenderProcess& operator=(const RenderProcess&) = delete;

 public:
  URGE_BINDING()
  using RenderCallback =
      base::RepeatingCallback<void(scoped_refptr<RenderContext> context,
                                   earray<scoped_refptr<Camera>> cameras)>;

  URGE_BINDING()
  static scoped_refptr<RenderProcess> New(RenderCallback callback,
                                          URGE_EXCEPTION);

 private:
  RenderCallback on_render_;
};

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
  URGE_ATTRIBUTE_DECLARE(WorldRoot, scoped_refptr<Node>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Process, scoped_refptr<RenderProcess>);

  URGE_BINDING()
  scoped_refptr<Vector2i> GetSize(URGE_EXCEPTION);

 private:
  glm::ivec2 size_;
  scoped_refptr<Node> root_;
  scoped_refptr<RenderProcess> process_;
};

}  // namespace content
