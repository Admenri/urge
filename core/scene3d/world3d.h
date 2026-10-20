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

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/scene/node.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/node3d.h"
#include "core/scene3d/render_graph3d.h"
#include "core/scene3d/render_target3d.h"

namespace urge {

URGE_BINDING()
class World3D : public Node {
 public:
  URGE_BINDING()
  World3D(int32_t x, int32_t y, int32_t width, int32_t height);
  URGE_BINDING()
  ~World3D() override;

  URGE_BINDING(Name : "x")
  ATTR(int32_t, X) {
    if (value.has_value())
      x_ = *value;
    return x_;
  }

  URGE_BINDING(Name : "y")
  ATTR(int32_t, Y) {
    if (value.has_value())
      y_ = *value;
    return y_;
  }

  URGE_BINDING(Name : "width")
  ATTR(int32_t, Width) {
    if (value.has_value())
      width_ = std::max(1, *value);
    return width_;
  }

  URGE_BINDING(Name : "height")
  ATTR(int32_t, Height) {
    if (value.has_value())
      height_ = std::max(1, *value);
    return height_;
  }

  URGE_BINDING(Name : "camera")
  ATTR(RefPtr<Camera3D>, Camera) {
    if (value.has_value())
      camera_ = *value;
    return camera_;
  }

  URGE_BINDING(Name : "graph")
  ATTR(RefPtr<RenderGraph3D>, Graph) {
    if (value.has_value())
      graph_ = *value;
    return graph_;
  }

  URGE_BINDING(Name : "root")
  RefPtr<Node3D> GetRoot() const { return root_; }
  URGE_BINDING(Name : "root=")
  void SetRoot(RefPtr<Node3D> value) { root_ = std::move(value); }

  URGE_BINDING(Name : "target")
  RefPtr<RenderTarget3D> GetTarget() const { return target_; }

 protected:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

 private:
  void RenderScene(DrawParam param);
  void EnsureQuadPipeline();
  void UpdateQuad(glm::ivec2 target_size);

  int32_t x_ = 0;
  int32_t y_ = 0;
  int32_t width_ = 1;
  int32_t height_ = 1;

  RefPtr<Camera3D> camera_;
  RefPtr<RenderGraph3D> graph_;
  RefPtr<Node3D> root_;
  RefPtr<RenderTarget3D> target_;

  wgpu::RenderPipeline quad_pipeline_ = nullptr;
  wgpu::Buffer quad_uniform_ = nullptr;
  wgpu::BindGroup quad_group_ = nullptr;
  wgpu::Sampler sampler_ = nullptr;
  wgpu::BindGroup texture_group_ = nullptr;
};

}
