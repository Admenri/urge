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
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/render/pipeline.h"
#include "core/render/wgsl_reflect.h"

namespace urge {

URGE_BINDING()
class MaterialPass3D : public Object {
 public:
  MaterialPass3D(std::string tag, std::string vs_wgsl, std::string fs_wgsl);
  ~MaterialPass3D() override;

  URGE_BINDING(Name : "tag")
  std::string GetTag() const { return tag_; }

  URGE_BINDING(Name : "depth_test")
  ATTR(bool, DepthTest) {
    if (value.has_value()) {
      if (*value != depth_test_) {
        depth_test_ = *value;
        pipeline_dirty_ = true;
      }
    }
    return depth_test_;
  }

  URGE_BINDING(Name : "depth_write")
  ATTR(bool, DepthWrite) {
    if (value.has_value()) {
      if (*value != depth_write_) {
        depth_write_ = *value;
        pipeline_dirty_ = true;
      }
    }
    return depth_write_;
  }

  URGE_BINDING(Name : "cull")
  ATTR(uint32_t, Cull) {
    if (value.has_value()) {
      if (*value != cull_) {
        cull_ = *value;
        pipeline_dirty_ = true;
      }
    }
    return cull_;
  }

  URGE_BINDING(Name : "skinned")
  ATTR(bool, Skinned) {
    if (value.has_value()) {
      if (*value != skinned_) {
        skinned_ = *value;
        pipeline_dirty_ = true;
      }
    }
    return skinned_;
  }

  URGE_BINDING(Name : "blend")
  std::string GetBlend() const { return blend_; }
  URGE_BINDING(Name : "blend=")
  void SetBlend(std::string value) {
    if (value != blend_) {
      blend_ = std::move(value);
      pipeline_dirty_ = true;
    }
  }

  URGE_BINDING(Name : "depth_only")
  ATTR(bool, DepthOnly) {
    if (value.has_value()) {
      if (*value != depth_only_) {
        depth_only_ = *value;
        pipeline_dirty_ = true;
      }
    }
    return depth_only_;
  }

  URGE_BINDING(Name : "params")
  std::vector<float> GetParams() const { return GetUniformSlot(0); }
  URGE_BINDING(Name : "params=")
  void SetParams(std::vector<float> value) { SetUniformSlot(0, value); }

  URGE_BINDING(Name : "texture")
  ATTR(RefPtr<Bitmap>, Texture) {
    if (value.has_value()) {
      SetTextureSlot(0, *value);
      return std::nullopt;
    }
    return GetTextureSlot(0);
  }

  URGE_BINDING()
  void SetTextureSlot(int32_t slot, RefPtr<Bitmap> texture);
  URGE_BINDING()
  RefPtr<Bitmap> GetTextureSlot(int32_t slot) const;
  URGE_BINDING(Name : "texture_slot_count")
  int32_t TextureSlotCount() const {
    return static_cast<int32_t>(texture_names_.size());
  }

  URGE_BINDING()
  void SetTexture(std::string name, RefPtr<Bitmap> texture);
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

  wgpu::RenderPipeline AcquirePipeline();
  wgpu::BindGroup AcquireBindGroup();
  bool HasGroup(uint32_t group) const;
  const std::string& tag() const { return tag_; }
  bool depth_only() const { return depth_only_; }
  void set_depth_only(bool value) { depth_only_ = value; }

 private:
  struct PipelineHolder : public RefCounted<PipelineHolder> {
    wgpu::RenderPipeline pipeline;
  };

  struct Slot {
    std::string name;
    uint32_t binding = 0;
    wgsl::BindingKind kind = wgsl::BindingKind::kUniform;
    uint64_t size = 0;
  };

  void RebuildPipeline();
  void RebuildBindGroup();
  void CollectSlots();

  std::string tag_;
  std::string vs_wgsl_;
  std::string fs_wgsl_;
  std::string blend_;

  bool depth_test_ = true;
  bool depth_write_ = true;
  bool depth_only_ = false;
  bool skinned_ = false;
  uint32_t group_count_ = 0;
  uint32_t cull_ = 3;
  bool pipeline_dirty_ = true;
  bool binding_dirty_ = true;

  std::vector<Slot> slots_;
  std::vector<std::string> texture_names_;
  std::vector<std::string> uniform_names_;
  std::map<std::string, RefPtr<Bitmap>> texture_map_;
  std::map<std::string, std::vector<float>> uniform_map_;
  std::map<std::string, wgpu::Buffer> uniform_buffers_;

  RefPtr<PipelineHolder> holder_;
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

 private:
  std::vector<RefPtr<MaterialPass3D>> passes_;
};

}
