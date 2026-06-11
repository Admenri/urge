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
class ShaderPass : public Object {
 public:
  URGE_BINDING()
  estring name = {};

  URGE_BINDING()
  emap<estring, estring> tags = {};

  URGE_BINDING()
  scoped_refptr<GPURenderPipeline> pipeline = nullptr;

  URGE_BINDING()
  uint32_t stencilRef = 0;
};

URGE_BINDING()
class Material : public Object {
 public:
  Material();

  Material(const Material&) = delete;
  Material& operator=(const Material&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<Material> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(RenderQueue, uint32_t);

  URGE_BINDING()
  void AddPass(scoped_refptr<ShaderPass> pass, URGE_EXCEPTION);

  URGE_BINDING()
  void RemovePass(estring name, URGE_EXCEPTION);

  URGE_BINDING()
  earray<estring> GetAllPassNames(URGE_EXCEPTION);

  URGE_BINDING()
  void SetupBindData(uint32_t slot,
                     scoped_refptr<GPUBindGroup> group,
                     earray<uint32_t> offsets,
                     URGE_EXCEPTION);

 private:
};

}  // namespace content
