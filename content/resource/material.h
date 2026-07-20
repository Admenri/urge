// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/gpu/gpu_device.h"
#include "content/gpu/gpu_resource.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class ShaderPass : public Object {
 public:
  URGE_BINDING()
  estring passName;

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

  uint32_t render_queue() { return render_queue_; }

  const std::vector<scoped_refptr<ShaderPass>>& passes() const {
    return passes_;
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Material> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(RenderQueue, uint32_t);

  URGE_BINDING()
  void SetupPasses(earray<scoped_refptr<ShaderPass>> passes, URGE_EXCEPTION);

  URGE_BINDING()
  earray<scoped_refptr<ShaderPass>> GetPasses(URGE_EXCEPTION);

  URGE_BINDING()
  void SetupBindingData(uint32_t slot,
                        scoped_refptr<GPUBindGroup> group,
                        earray<uint32_t> offsets,
                        URGE_EXCEPTION);

 private:
  struct BindData {
    scoped_refptr<GPUBindGroup> bind_group;
    std::vector<uint32_t> offsets;
  };

  uint32_t render_queue_;
  std::vector<scoped_refptr<ShaderPass>> passes_;
  std::vector<BindData> bindings_;
};

}  // namespace content
