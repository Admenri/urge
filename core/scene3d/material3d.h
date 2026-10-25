// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/render/pipeline.h"
#include "core/render/shader.h"
#include "core/render/wgsl_reflect.h"

namespace urge {

class GPUTextureView;

struct PipelineTargetState3D {
  std::vector<wgpu::TextureFormat> colors;
  wgpu::TextureFormat depth = wgpu::TextureFormat::Undefined;
  uint32_t samples = 1;
  int32_t cull = -1;
  bool flip_winding = false;

  std::string Key() const;
};

URGE_BINDING()
class MaterialPass3D : public Object {
 public:
  MaterialPass3D(std::string tag, std::string vs_wgsl, std::string fs_wgsl);
  ~MaterialPass3D() override;

  URGE_BINDING(Name : "tag")
  std::string GetTag() const { return tag_; }

  URGE_BINDING(Name : "tag_id")
  ATTR(int32_t, TagId) {
    if (value.has_value())
      tag_id_ = *value;
    return tag_id_;
  }

  URGE_BINDING(Name : "queue")
  ATTR(int32_t, Queue) {
    if (value.has_value())
      queue_ = *value;
    return queue_;
  }

  URGE_BINDING(Name : "vertex_entry")
  std::string GetVertexEntry() const { return vertex_entry_; }
  URGE_BINDING(Name : "vertex_entry=")
  void SetVertexEntry(std::string value) {
    if (value != vertex_entry_) {
      vertex_entry_ = std::move(value);
      state_dirty_ = true;
    }
  }

  URGE_BINDING(Name : "fragment_entry")
  std::string GetFragmentEntry() const { return fragment_entry_; }
  URGE_BINDING(Name : "fragment_entry=")
  void SetFragmentEntry(std::string value) {
    if (value != fragment_entry_) {
      fragment_entry_ = std::move(value);
      state_dirty_ = true;
    }
  }

  URGE_BINDING(Name : "skinned")
  ATTR(bool, Skinned) {
    if (value.has_value()) {
      if (*value != skinned_) {
        skinned_ = *value;
        state_dirty_ = true;
      }
    }
    return skinned_;
  }

  URGE_BINDING(Name : "instanced")
  ATTR(bool, Instanced) {
    if (value.has_value()) {
      if (*value != instanced_) {
        instanced_ = *value;
        state_dirty_ = true;
      }
    }
    return instanced_;
  }

  URGE_BINDING(Name : "fullscreen")
  ATTR(bool, Fullscreen) {
    if (value.has_value()) {
      if (*value != fullscreen_) {
        fullscreen_ = *value;
        state_dirty_ = true;
      }
    }
    return fullscreen_;
  }

  URGE_BINDING(Name : "depth_only")
  ATTR(bool, DepthOnly) {
    if (value.has_value()) {
      if (*value != depth_only_) {
        depth_only_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_only_;
  }

  URGE_BINDING(Name : "topology")
  ATTR(uint32_t, Topology) {
    if (value.has_value()) {
      if (*value != topology_) {
        topology_ = *value;
        state_dirty_ = true;
      }
    }
    return topology_;
  }

  URGE_BINDING(Name : "front_face")
  ATTR(uint32_t, FrontFace) {
    if (value.has_value()) {
      if (*value != front_face_) {
        front_face_ = *value;
        state_dirty_ = true;
      }
    }
    return front_face_;
  }

  URGE_BINDING(Name : "cull")
  ATTR(uint32_t, Cull) {
    if (value.has_value()) {
      if (*value != cull_) {
        cull_ = *value;
        state_dirty_ = true;
      }
    }
    return cull_;
  }

  URGE_BINDING(Name : "depth_test")
  ATTR(bool, DepthTest) {
    if (value.has_value()) {
      if (*value != depth_test_) {
        depth_test_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_test_;
  }

  URGE_BINDING(Name : "depth_write")
  ATTR(bool, DepthWrite) {
    if (value.has_value()) {
      if (*value != depth_write_) {
        depth_write_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_write_;
  }

  URGE_BINDING(Name : "depth_compare")
  ATTR(uint32_t, DepthCompare) {
    if (value.has_value()) {
      if (*value != depth_compare_) {
        depth_compare_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_compare_;
  }

  URGE_BINDING(Name : "depth_bias")
  ATTR(int32_t, DepthBias) {
    if (value.has_value()) {
      if (*value != depth_bias_) {
        depth_bias_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_bias_;
  }

  URGE_BINDING(Name : "depth_bias_slope")
  ATTR(float, DepthBiasSlope) {
    if (value.has_value()) {
      if (*value != depth_bias_slope_) {
        depth_bias_slope_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_bias_slope_;
  }

  URGE_BINDING(Name : "depth_bias_clamp")
  ATTR(float, DepthBiasClamp) {
    if (value.has_value()) {
      if (*value != depth_bias_clamp_) {
        depth_bias_clamp_ = *value;
        state_dirty_ = true;
      }
    }
    return depth_bias_clamp_;
  }

  URGE_BINDING(Name : "stencil")
  ATTR(bool, Stencil) {
    if (value.has_value()) {
      if (*value != stencil_) {
        stencil_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_;
  }

  URGE_BINDING(Name : "stencil_ref")
  ATTR(uint32_t, StencilRef) {
    if (value.has_value())
      stencil_ref_ = *value;
    return stencil_ref_;
  }

  URGE_BINDING(Name : "stencil_read_mask")
  ATTR(uint32_t, StencilReadMask) {
    if (value.has_value()) {
      if (*value != stencil_read_mask_) {
        stencil_read_mask_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_read_mask_;
  }

  URGE_BINDING(Name : "stencil_write_mask")
  ATTR(uint32_t, StencilWriteMask) {
    if (value.has_value()) {
      if (*value != stencil_write_mask_) {
        stencil_write_mask_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_write_mask_;
  }

  URGE_BINDING(Name : "stencil_front_compare")
  ATTR(uint32_t, StencilFrontCompare) {
    if (value.has_value()) {
      if (*value != stencil_front_compare_) {
        stencil_front_compare_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_front_compare_;
  }

  URGE_BINDING(Name : "stencil_front_fail")
  ATTR(uint32_t, StencilFrontFail) {
    if (value.has_value()) {
      if (*value != stencil_front_fail_) {
        stencil_front_fail_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_front_fail_;
  }

  URGE_BINDING(Name : "stencil_front_depth_fail")
  ATTR(uint32_t, StencilFrontDepthFail) {
    if (value.has_value()) {
      if (*value != stencil_front_depth_fail_) {
        stencil_front_depth_fail_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_front_depth_fail_;
  }

  URGE_BINDING(Name : "stencil_front_pass")
  ATTR(uint32_t, StencilFrontPass) {
    if (value.has_value()) {
      if (*value != stencil_front_pass_) {
        stencil_front_pass_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_front_pass_;
  }

  URGE_BINDING(Name : "stencil_back_compare")
  ATTR(uint32_t, StencilBackCompare) {
    if (value.has_value()) {
      if (*value != stencil_back_compare_) {
        stencil_back_compare_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_back_compare_;
  }

  URGE_BINDING(Name : "stencil_back_fail")
  ATTR(uint32_t, StencilBackFail) {
    if (value.has_value()) {
      if (*value != stencil_back_fail_) {
        stencil_back_fail_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_back_fail_;
  }

  URGE_BINDING(Name : "stencil_back_depth_fail")
  ATTR(uint32_t, StencilBackDepthFail) {
    if (value.has_value()) {
      if (*value != stencil_back_depth_fail_) {
        stencil_back_depth_fail_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_back_depth_fail_;
  }

  URGE_BINDING(Name : "stencil_back_pass")
  ATTR(uint32_t, StencilBackPass) {
    if (value.has_value()) {
      if (*value != stencil_back_pass_) {
        stencil_back_pass_ = *value;
        state_dirty_ = true;
      }
    }
    return stencil_back_pass_;
  }

  URGE_BINDING(Name : "blend")
  std::string GetBlend() const { return blend_; }
  URGE_BINDING(Name : "blend=")
  void SetBlend(std::string value) {
    if (value != blend_) {
      blend_ = std::move(value);
      state_dirty_ = true;
    }
  }

  URGE_BINDING(Name : "color_write_mask")
  ATTR(uint32_t, ColorWriteMask) {
    if (value.has_value()) {
      if (*value != color_write_mask_) {
        color_write_mask_ = *value;
        state_dirty_ = true;
      }
    }
    return color_write_mask_;
  }

  URGE_BINDING(Name : "alpha_to_coverage")
  ATTR(bool, AlphaToCoverage) {
    if (value.has_value()) {
      if (*value != alpha_to_coverage_) {
        alpha_to_coverage_ = *value;
        state_dirty_ = true;
      }
    }
    return alpha_to_coverage_;
  }

  URGE_BINDING(Name : "params")
  std::vector<float> GetParams() const { return GetUniformSlot(0); }
  URGE_BINDING(Name : "params=")
  void SetParams(std::vector<float> value) { SetUniformSlot(0, value); }

  URGE_BINDING(Name : "texture")
  ATTR(MaybePtr<Bitmap>, Texture) {
    if (value.has_value()) {
      SetTextureSlot(0, *value);
      return std::nullopt;
    }
    return GetTextureSlot(0);
  }

  URGE_BINDING()
  void SetTextureSlot(int32_t slot, MaybePtr<Bitmap> texture);
  URGE_BINDING()
  RefPtr<Bitmap> GetTextureSlot(int32_t slot) const;
  URGE_BINDING(Name : "texture_slot_count")
  int32_t TextureSlotCount() const {
    return static_cast<int32_t>(texture_names_.size());
  }

  URGE_BINDING()
  void SetTexture(std::string name, MaybePtr<Bitmap> texture);
  void SetTextureView(std::string name, RefPtr<GPUTextureView> view);
  URGE_BINDING()
  RefPtr<Bitmap> GetTexture(std::string name) const;

  URGE_BINDING()
  void SetUniformSlot(int32_t slot, std::vector<float> values);
  URGE_BINDING()
  std::vector<float> GetUniformSlot(int32_t slot) const;
  URGE_BINDING()
  void SetUniform(std::string name, std::vector<float> values);
  URGE_BINDING()
  std::vector<float> GetUniform(std::string name) const;
  URGE_BINDING(Name : "uniform_slot_count")
  int32_t UniformSlotCount() const {
    return static_cast<int32_t>(uniform_names_.size());
  }

  URGE_BINDING()
  std::vector<std::string> SlotNames() const;

  wgpu::RenderPipeline AcquirePipeline(const PipelineTargetState3D& target);
  wgpu::RenderPipeline AcquireDepthOnlyPipeline(
      const PipelineTargetState3D& target);
  wgpu::BindGroup AcquireBindGroup();
  bool HasGroup(uint32_t group) const;
  const std::string& tag() const { return tag_; }
  int32_t tag_id() const { return tag_id_; }
  int32_t queue() const { return queue_; }
  bool depth_only() const { return depth_only_; }
  bool fullscreen() const { return fullscreen_; }
  void set_fullscreen(bool value) {
    if (fullscreen_ == value)
      return;
    fullscreen_ = value;
    state_dirty_ = true;
  }
  bool skinned() const { return skinned_; }
  bool instanced() const { return instanced_; }
  uint32_t stencil_ref() const { return stencil_ref_; }
  bool stencil() const { return stencil_; }
  const std::string& blend() const { return blend_; }
  void set_depth_only(bool value) {
    depth_only_ = value;
    state_dirty_ = true;
  }
  void SetSources(std::string vs_wgsl, std::string fs_wgsl);

 private:
  struct Slot {
    std::string name;
    uint32_t binding = 0;
    wgsl::BindingKind kind = wgsl::BindingKind::kUniform;
    uint64_t size = 0;
  };

  void CollectSlots();
  void RebuildBindGroup();
  void EnsureBuilder();
  void EnsureShadowBuilder();
  wgpu::DepthStencilState MakeDepthState(wgpu::TextureFormat format) const;
  std::vector<std::vector<VertexAttribute>> VertexLayout() const;

  std::string tag_;
  std::string vs_wgsl_;
  std::string fs_wgsl_;
  std::string vertex_entry_ = "vs_main";
  std::string fragment_entry_ = "fs_main";
  std::string blend_;

  int32_t tag_id_ = 0;
  int32_t queue_ = 2000;
  bool skinned_ = false;
  bool instanced_ = false;
  bool depth_only_ = false;
  bool fullscreen_ = false;
  uint32_t topology_ = 4;
  uint32_t front_face_ = 1;
  uint32_t cull_ = 3;
  bool depth_test_ = true;
  bool depth_write_ = true;
  uint32_t depth_compare_ = 4;
  int32_t depth_bias_ = 0;
  float depth_bias_slope_ = 0.0f;
  float depth_bias_clamp_ = 0.0f;
  bool stencil_ = false;
  uint32_t stencil_ref_ = 0;
  uint32_t stencil_read_mask_ = 0xFF;
  uint32_t stencil_write_mask_ = 0xFF;
  uint32_t stencil_front_compare_ = 8;
  uint32_t stencil_front_fail_ = 0;
  uint32_t stencil_front_depth_fail_ = 0;
  uint32_t stencil_front_pass_ = 0;
  uint32_t stencil_back_compare_ = 8;
  uint32_t stencil_back_fail_ = 0;
  uint32_t stencil_back_depth_fail_ = 0;
  uint32_t stencil_back_pass_ = 0;
  uint32_t color_write_mask_ = 0xF;
  bool alpha_to_coverage_ = false;

  uint32_t group_count_ = 0;
  bool state_dirty_ = true;
  bool binding_dirty_ = true;

  std::vector<Slot> slots_;
  std::vector<std::string> texture_names_;
  std::vector<std::string> uniform_names_;
  std::map<std::string, RefPtr<Bitmap>> texture_map_;
  std::map<std::string, RefPtr<GPUTextureView>> view_map_;
  std::map<std::string, std::vector<float>> uniform_map_;
  std::map<std::string, wgpu::Buffer> uniform_buffers_;

  std::unique_ptr<Pipeline> builder_;
  std::unique_ptr<Pipeline> shadow_builder_;
  wgpu::BindGroupLayout material_layout_ = nullptr;
  std::vector<std::pair<std::string, wgpu::RenderPipeline>> pipelines_;
  std::vector<std::pair<std::string, wgpu::RenderPipeline>> shadow_pipelines_;
  wgpu::BindGroup binding_ = nullptr;
};

URGE_BINDING()
class Material3D : public Object {
 public:
  URGE_BINDING()
  Material3D();
  ~Material3D() override;

  URGE_BINDING()
  RefPtr<MaterialPass3D> AddPass(std::string tag,
                                 std::string vs_wgsl,
                                 std::string fs_wgsl);
  URGE_BINDING()
  RefPtr<MaterialPass3D> AddShadowPass(std::string tag, std::string vs_wgsl);
  URGE_BINDING()
  RefPtr<MaterialPass3D> GetPass(std::string tag);
  URGE_BINDING()
  bool HasPass(std::string tag);
  URGE_BINDING()
  void RemovePass(std::string tag);
  URGE_BINDING()
  int32_t PassCount() const { return static_cast<int32_t>(passes_.size()); }
  URGE_BINDING()
  std::vector<std::string> Tags() const;
  URGE_BINDING()
  RefPtr<MaterialPass3D> PassAt(int32_t index) const;

 private:
  std::vector<RefPtr<MaterialPass3D>> passes_;
};

}
