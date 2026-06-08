// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/gpu/gpu_device.h"
#include "content/gpu/gpu_resource.h"
#include "content/render/camera.h"
#include "content/scene/node.h"

namespace content {

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
  void DrawRenderers(scoped_refptr<GPURenderPassEncoder> pass, URGE_EXCEPTION);

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
