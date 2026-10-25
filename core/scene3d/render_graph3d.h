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

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/render/wgsl_reflect.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/light3d.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/render_target3d.h"

namespace urge {

class GPURenderPassEncoder;
class GPUTextureView;
class RenderContext3D;
class RenderGraph3D;

URGE_BINDING()
class ComputePass3D : public Object {
 public:
  URGE_BINDING()
  ComputePass3D(std::string wgsl, std::string entry);
  ~ComputePass3D() override;

  URGE_BINDING()
  void SetUniform(std::string name, std::vector<float> values);
  URGE_BINDING()
  void SetBuffer(std::string name, std::vector<float> values);
  URGE_BINDING()
  void SetTexture(std::string name, MaybePtr<Bitmap> texture);
  URGE_BINDING()
  void SetTarget(std::string name,
                 RefPtr<RenderTarget3D> target,
                 int32_t index);

  URGE_BINDING(Name : "dispatch_x")
  ATTR(int32_t, DispatchX) {
    if (value.has_value())
      dispatch_x_ = std::max(0, *value);
    return dispatch_x_;
  }
  URGE_BINDING(Name : "dispatch_y")
  ATTR(int32_t, DispatchY) {
    if (value.has_value())
      dispatch_y_ = std::max(0, *value);
    return dispatch_y_;
  }
  URGE_BINDING(Name : "dispatch_z")
  ATTR(int32_t, DispatchZ) {
    if (value.has_value())
      dispatch_z_ = std::max(0, *value);
    return dispatch_z_;
  }

  void Execute(RenderContext3D* context);

 private:
  struct Slot {
    std::string name;
    uint32_t binding = 0;
    wgsl::BindingKind kind = wgsl::BindingKind::kUniform;
    uint64_t size = 0;
  };

  void EnsurePipeline();
  void RebuildBindGroup();

  std::string wgsl_;
  std::string entry_;
  int32_t dispatch_x_ = 1;
  int32_t dispatch_y_ = 1;
  int32_t dispatch_z_ = 1;

  std::vector<Slot> slots_;
  std::map<std::string, std::vector<float>> uniform_map_;
  std::map<std::string, RefPtr<Bitmap>> texture_map_;
  std::map<std::string, std::pair<RefPtr<RenderTarget3D>, int32_t>> target_map_;
  std::map<std::string, wgpu::Buffer> uniform_buffers_;
  std::map<std::string, std::vector<float>> buffer_data_;
  std::map<std::string, wgpu::Buffer> storage_buffers_;

  wgpu::ComputePipeline pipeline_ = nullptr;
  wgpu::BindGroup binding_ = nullptr;
  bool binding_dirty_ = true;
};

URGE_BINDING()
class RenderGraphPass3D : public Object {
 public:
  explicit RenderGraphPass3D(std::string name);
  ~RenderGraphPass3D() override;

  URGE_BINDING(Name : "name")
  std::string GetName() const { return name_; }

  URGE_BINDING(Name : "enabled")
  ATTR(bool, Enabled) {
    if (value.has_value())
      enabled_ = *value;
    return enabled_;
  }

  URGE_BINDING(Name : "kind")
  ATTR(uint32_t, Kind) {
    if (value.has_value())
      kind_ = *value;
    return kind_;
  }

  URGE_BINDING(Name : "colors")
  std::vector<std::string> GetColors() const { return colors_; }
  URGE_BINDING(Name : "colors=")
  void SetColors(std::vector<std::string> value) { colors_ = std::move(value); }

  URGE_BINDING(Name : "clear_color")
  RefPtr<Color> GetClearColor() const { return clear_color_; }
  URGE_BINDING(Name : "clear_color=")
  void SetClearColor(MaybePtr<Color> value) { clear_color_ = std::move(value); }

  URGE_BINDING(Name : "clear_flags")
  ATTR(uint32_t, ClearFlags) {
    if (value.has_value())
      clear_flags_ = *value;
    return clear_flags_;
  }

  URGE_BINDING(Name : "cull_override")
  ATTR(int32_t, CullOverride) {
    if (value.has_value())
      cull_override_ = *value;
    return cull_override_;
  }

  URGE_BINDING(Name : "sort_back_to_front")
  ATTR(bool, SortBackToFront) {
    if (value.has_value())
      sort_back_to_front_ = *value;
    return sort_back_to_front_;
  }

  URGE_BINDING(Name : "depth_only")
  ATTR(bool, DepthOnly) {
    if (value.has_value())
      depth_only_ = *value;
    return depth_only_;
  }

  URGE_BINDING(Name : "camera")
  ATTR(MaybePtr<Camera3D>, Camera) {
    if (value.has_value())
      camera_ = *value;
    return camera_;
  }

  URGE_BINDING(Name : "layers")
  ATTR(uint32_t, Layers) {
    if (value.has_value())
      layers_ = *value;
    return layers_;
  }

  URGE_BINDING(Name : "queue_min")
  ATTR(int32_t, QueueMin) {
    if (value.has_value())
      queue_min_ = *value;
    return queue_min_;
  }

  URGE_BINDING(Name : "queue_max")
  ATTR(int32_t, QueueMax) {
    if (value.has_value())
      queue_max_ = *value;
    return queue_max_;
  }

  URGE_BINDING(Name : "tags")
  std::vector<std::string> GetTags() const { return tags_; }
  URGE_BINDING(Name : "tags=")
  void SetTags(std::vector<std::string> value) { tags_ = std::move(value); }

  URGE_BINDING(Name : "tag_ids")
  std::vector<int32_t> GetTagIds() const { return tag_ids_; }
  URGE_BINDING(Name : "tag_ids=")
  void SetTagIds(std::vector<int32_t> value) { tag_ids_ = std::move(value); }

  URGE_BINDING()
  void BindTexture(std::string slot, std::string texture);
  URGE_BINDING(Name : "bindings")
  std::vector<std::string> GetBindings() const;

  URGE_BINDING(Name : "reads")
  std::vector<std::string> GetReads() const { return reads_; }
  URGE_BINDING(Name : "reads=")
  void SetReads(std::vector<std::string> value) { reads_ = std::move(value); }

  URGE_BINDING(Name : "material")
  RefPtr<MaterialPass3D> GetMaterial() const { return material_; }
  URGE_BINDING(Name : "material=")
  void SetMaterial(MaybePtr<MaterialPass3D> value) {
    material_ = std::move(value);
  }

  URGE_BINDING(Name : "compute")
  RefPtr<ComputePass3D> GetCompute() const { return compute_; }
  URGE_BINDING(Name : "compute=")
  void SetCompute(MaybePtr<ComputePass3D> value) { compute_ = std::move(value); }

  URGE_BINDING(Name : "cascades")
  ATTR(int32_t, Cascades) {
    if (value.has_value())
      cascades_ = std::clamp(*value, 1, 4);
    return cascades_;
  }

  URGE_BINDING(Name : "shadow_light")
  ATTR(MaybePtr<Light3D>, ShadowLight) {
    if (value.has_value())
      shadow_light_ = *value;
    return shadow_light_;
  }

  URGE_BINDING(Name : "point_shadow_light")
  ATTR(MaybePtr<Light3D>, PointShadowLight) {
    if (value.has_value())
      point_shadow_light_ = *value;
    return point_shadow_light_;
  }

  URGE_BINDING(Name : "cascade_fade")
  ATTR(float, CascadeFade) {
    if (value.has_value())
      cascade_fade_ = std::clamp(*value, 0.0f, 0.5f);
    return cascade_fade_;
  }

  URGE_BINDING(Name : "split_lambda")
  ATTR(float, SplitLambda) {
    if (value.has_value())
      split_lambda_ = std::clamp(*value, 0.0f, 1.0f);
    return split_lambda_;
  }

  URGE_BINDING(Name : "shadow_distance")
  ATTR(float, ShadowDistance) {
    if (value.has_value())
      shadow_distance_ = std::max(*value, 1.0f);
    return shadow_distance_;
  }

  URGE_BINDING(Name : "texture")
  RefPtr<GPUTextureView> GetTexture(std::string name);
  URGE_BINDING(Name : "pass")
  RefPtr<GPURenderPassEncoder> GetPass();
  URGE_BINDING(Name : "context")
  RefPtr<RenderContext3D> GetContext();
  URGE_BINDING(Name : "width")
  int32_t Width();
  URGE_BINDING(Name : "height")
  int32_t Height();

  void SetOwner(RenderGraph3D* graph) { graph_ = graph; }
  void SetContext(RenderContext3D* context) { context_ = context; }

  const std::string& name() const { return name_; }
  uint32_t kind() const { return kind_; }
  const std::vector<std::string>& colors() const { return colors_; }
  const std::vector<std::string>& tags() const { return tags_; }
  const std::vector<int32_t>& tag_ids() const { return tag_ids_; }
  const std::vector<std::string>& reads() const { return reads_; }
  const std::map<std::string, std::string>& bindings() const {
    return bindings_;
  }
  bool enabled() const { return enabled_; }
  bool depth_only() const { return depth_only_; }
  bool sort_back_to_front() const { return sort_back_to_front_; }
  uint32_t layers() const { return layers_; }
  uint32_t clear_flags() const { return clear_flags_; }
  int32_t queue_min() const { return queue_min_; }
  int32_t cull_override() const { return cull_override_; }
  int32_t queue_max() const { return queue_max_; }
  int32_t cascades() const { return cascades_; }
  float split_lambda() const { return split_lambda_; }
  float cascade_fade() const { return cascade_fade_; }
  float shadow_distance() const { return shadow_distance_; }
  RefPtr<Camera3D> camera() const { return camera_; }
  RefPtr<Light3D> shadow_light() const { return shadow_light_; }
  RefPtr<Light3D> point_shadow_light() const { return point_shadow_light_; }
  RefPtr<Color> clear_color() const { return clear_color_; }
  RefPtr<MaterialPass3D> material() const { return material_; }
  RefPtr<ComputePass3D> compute() const { return compute_; }

 private:
  std::string name_;
  bool enabled_ = true;
  uint32_t kind_ = 0;
  std::vector<std::string> colors_;
  RefPtr<Color> clear_color_ =
      MakeRefCounted<Color>(26.0f, 28.0f, 36.0f, 255.0f);
  uint32_t clear_flags_ = 3;
  std::vector<std::string> tags_;
  std::vector<int32_t> tag_ids_;
  std::vector<std::string> reads_;
  std::map<std::string, std::string> bindings_;
  bool sort_back_to_front_ = false;
  bool depth_only_ = false;
  uint32_t layers_ = 0;
  int32_t cull_override_ = -1;
  int32_t queue_min_ = 0;
  int32_t queue_max_ = 4000;
  RefPtr<Camera3D> camera_;
  RefPtr<Light3D> shadow_light_;
  RefPtr<Light3D> point_shadow_light_;
  int32_t cascades_ = 1;
  float split_lambda_ = 0.6f;
  float cascade_fade_ = 0.15f;
  float shadow_distance_ = 60.0f;
  RefPtr<MaterialPass3D> material_;
  RefPtr<ComputePass3D> compute_;

  RenderGraph3D* graph_ = nullptr;
  RenderContext3D* context_ = nullptr;
};

URGE_BINDING()
class RenderGraph3D : public Object {
 public:
  URGE_BINDING()
  RenderGraph3D();
  ~RenderGraph3D() override;

  URGE_BINDING()
  RefPtr<RenderGraphPass3D> AddPass(std::string name);
  URGE_BINDING()
  RefPtr<RenderGraphPass3D> GetPass(std::string name);
  URGE_BINDING()
  void RemovePass(std::string name);
  URGE_BINDING()
  void Clear();
  URGE_BINDING()
  int32_t PassCount() const { return static_cast<int32_t>(passes_.size()); }
  URGE_BINDING()
  RefPtr<RenderGraphPass3D> PassAt(int32_t index) const;

  URGE_BINDING()
  void AddTexture(std::string name, std::string format, int32_t samples);
  URGE_BINDING()
  void AddStorageTexture(std::string name, std::string format);
  URGE_BINDING()
  void SetTextureScale(std::string name, float scale);
  URGE_BINDING()
  RefPtr<RenderTarget3D> GetTexture(std::string name);
  URGE_BINDING()
  std::vector<std::string> TextureNames() const;

  URGE_BINDING(Name : "ambient")
  RefPtr<Color> GetAmbient() const { return ambient_; }
  URGE_BINDING(Name : "ambient=")
  void SetAmbient(MaybePtr<Color> value) { ambient_ = std::move(value); }

  URGE_BINDING(Name : "shadow_size")
  ATTR(int32_t, ShadowSize) {
    if (value.has_value()) {
      const int32_t size = std::max(16, *value);
      if (size != shadow_size_) {
        shadow_size_ = size;
        shadow_targets_.clear();
      }
    }
    return shadow_size_;
  }

  URGE_BINDING(Name : "camera")
  ATTR(MaybePtr<Camera3D>, Camera) {
    if (value.has_value())
      camera_ = *value;
    return camera_;
  }

  void Execute(RenderContext3D* context);
  RefPtr<RenderTarget3D> Target(const std::string& name);
  RefPtr<RenderTarget3D> TargetForPass(RenderGraphPass3D* pass);
  wgpu::TextureView ResolveColorView(const std::string& name);

 private:
  struct TextureDesc {
    wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
    int32_t samples = 1;
    float scale = 1.0f;
    bool storage = false;
  };

  std::vector<size_t> BuildOrder() const;
  RefPtr<RenderTarget3D> ShadowTarget(const std::string& name, int32_t layers);
  void ExecuteShadowPass(RenderContext3D* context, RenderGraphPass3D* pass);
  void ExecuteRasterPass(RenderContext3D* context, RenderGraphPass3D* pass);
  void ExecuteFullscreenPass(RenderContext3D* context, RenderGraphPass3D* pass);

  std::vector<RefPtr<RenderGraphPass3D>> passes_;
  std::map<std::string, RefPtr<RenderTarget3D>> targets_;
  std::map<std::string, RefPtr<RenderTarget3D>> shadow_targets_;
  std::map<std::string, RefPtr<RenderTarget3D>> mrt_targets_;
  std::map<std::string, std::pair<RefPtr<RenderTarget3D>, int32_t>>
      attachment_views_;
  std::map<std::string, TextureDesc> textures_;
  RefPtr<RenderTarget3D> final_target_;
  RefPtr<Color> ambient_;
  RefPtr<Camera3D> camera_;
  int32_t shadow_size_ = 1024;
};

}
