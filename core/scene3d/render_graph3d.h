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

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/graphics/gpu.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/light3d.h"

namespace urge {

class RenderContext3D;
class RenderTarget3D;
class RenderGraph3D;
class RenderGraphPass3D;

class GraphPassProc3D : public RefCounted<GraphPassProc3D> {
 public:
  virtual ~GraphPassProc3D() = default;
  virtual void Invoke(RenderGraphPass3D* pass) = 0;
};

URGE_BINDING()
class RenderGraphPass3D : public Object {
 public:
  explicit RenderGraphPass3D(std::string name);

  URGE_BINDING(Name : "name")
  std::string GetName() const { return name_; }

  URGE_BINDING(Name : "color")
  std::string GetColor() const { return color_; }
  URGE_BINDING(Name : "color=")
  void SetColor(std::string value) { color_ = std::move(value); }

  URGE_BINDING(Name : "clear_color")
  RefPtr<Color> GetClearColor() const { return clear_color_; }
  URGE_BINDING(Name : "clear_color=")
  void SetClearColor(RefPtr<Color> value) { clear_color_ = std::move(value); }

  URGE_BINDING(Name : "clear_depth")
  ATTR(bool, ClearDepth) {
    if (value.has_value())
      clear_depth_ = *value;
    return clear_depth_;
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
  ATTR(RefPtr<Camera3D>, Camera) {
    if (value.has_value())
      camera_ = *value;
    return camera_;
  }

  URGE_BINDING(Name : "cascades")
  ATTR(int32_t, Cascades) {
    if (value.has_value())
      cascades_ = std::clamp(*value, 1, 4);
    return cascades_;
  }

  URGE_BINDING(Name : "shadow_light")
  ATTR(RefPtr<Light3D>, ShadowLight) {
    if (value.has_value())
      shadow_light_ = *value;
    return shadow_light_;
  }

  URGE_BINDING(Name : "point_shadow_light")
  ATTR(RefPtr<Light3D>, PointShadowLight) {
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

  URGE_BINDING(Name : "tags")
  std::vector<std::string> GetTags() const { return tags_; }
  URGE_BINDING(Name : "tags=")
  void SetTags(std::vector<std::string> value) { tags_ = std::move(value); }

  URGE_BINDING(Name : "reads")
  std::vector<std::string> GetReads() const { return reads_; }
  URGE_BINDING(Name : "reads=")
  void SetReads(std::vector<std::string> value) { reads_ = std::move(value); }

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

  void SetProc(RefPtr<GraphPassProc3D> proc) { proc_ = std::move(proc); }
  RefPtr<GraphPassProc3D> GetProc() const { return proc_; }
  void SetOwner(RenderGraph3D* graph) { graph_ = graph; }
  void SetContext(RenderContext3D* context) { context_ = context; }

  const std::string& color() const { return color_; }
  const std::vector<std::string>& tags() const { return tags_; }
  const std::vector<std::string>& reads() const { return reads_; }
  bool clear_depth() const { return clear_depth_; }
  bool depth_only() const { return depth_only_; }
  RefPtr<Camera3D> camera() const { return camera_; }
  int32_t cascades() const { return cascades_; }
  RefPtr<Light3D> shadow_light() const { return shadow_light_; }
  float split_lambda() const { return split_lambda_; }
  float cascade_fade() const { return cascade_fade_; }
  RefPtr<Light3D> point_shadow_light() const { return point_shadow_light_; }
  float shadow_distance() const { return shadow_distance_; }
  bool sort_back_to_front() const { return sort_back_to_front_; }
  const RefPtr<Color>& clear_color() const { return clear_color_; }

 private:
  std::string name_;
  std::string color_;
  RefPtr<Color> clear_color_;
  std::vector<std::string> tags_;
  std::vector<std::string> reads_;
  bool clear_depth_ = true;
  bool sort_back_to_front_ = false;
  bool depth_only_ = false;
  RefPtr<Camera3D> camera_;
  RefPtr<Light3D> shadow_light_;
  RefPtr<Light3D> point_shadow_light_;
  int32_t cascades_ = 1;
  float split_lambda_ = 0.6f;
  float cascade_fade_ = 0.15f;
  float shadow_distance_ = 60.0f;

  RefPtr<GraphPassProc3D> proc_;
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

  URGE_BINDING(Name : "ambient")
  RefPtr<Color> GetAmbient() const { return ambient_; }
  URGE_BINDING(Name : "ambient=")
  void SetAmbient(RefPtr<Color> value) { ambient_ = std::move(value); }

  URGE_BINDING(Name : "shadow_size")
  ATTR(int32_t, ShadowSize) {
    if (value.has_value())
      shadow_size_ = std::max(16, *value);
    return shadow_size_;
  }

  URGE_BINDING(Name : "camera")
  ATTR(RefPtr<Camera3D>, Camera) {
    if (value.has_value())
      camera_ = *value;
    return camera_;
  }

  void Execute(RenderContext3D* context);
  RefPtr<RenderTarget3D> Target(const std::string& name);
  RefPtr<RenderTarget3D> ShadowTarget(const std::string& name,
                                      int32_t layers);

 private:
  std::vector<size_t> BuildOrder() const;

  std::vector<RefPtr<RenderGraphPass3D>> passes_;
  std::map<std::string, RefPtr<RenderTarget3D>> targets_;
  std::map<std::string, RefPtr<RenderTarget3D>> shadow_targets_;
  RefPtr<RenderTarget3D> final_target_;
  RefPtr<Color> ambient_;
  RefPtr<Camera3D> camera_;
  int32_t shadow_size_ = 1024;
};

}
