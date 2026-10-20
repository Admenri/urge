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
#include <optional>
#include <vector>

#include "glm/glm.hpp"

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"

namespace urge {

class GPUTextureView;

URGE_BINDING()
class RenderTarget3D : public Object {
 public:
  RenderTarget3D(int32_t width, int32_t height, bool with_color);
  RenderTarget3D(int32_t width, int32_t height, bool with_color, int32_t layers);
  ~RenderTarget3D() override;

  URGE_BINDING()
  int32_t Width() const { return size_.x; }
  URGE_BINDING()
  int32_t Height() const { return size_.y; }
  URGE_BINDING()
  bool HasColor() const { return color_view_ != nullptr; }
  URGE_BINDING(Name : "layers")
  int32_t LayerCount() const { return layers_; }

  glm::ivec2 size() const { return size_; }

  wgpu::RenderPassEncoder BeginRendering(wgpu::CommandEncoder encoder,
                                         std::optional<glm::vec4> clear,
                                         bool clear_depth,
                                         int32_t layer = 0);

  wgpu::TextureView color_view() const { return color_view_; }
  wgpu::TextureView depth_view() const { return layer_views_.empty() ? nullptr : layer_views_[0]; }
  wgpu::TextureView depth_view(int32_t layer) const;
  wgpu::TextureView depth_array_view() const { return depth_array_view_; }
  wgpu::TextureView depth_cube_view() const { return depth_cube_view_; }
  wgpu::Texture depth_texture() const { return depth_texture_; }

 private:
  void CreateResources(int32_t width, int32_t height, bool with_color,
                       int32_t layers);

  glm::ivec2 size_ = glm::ivec2(1);
  int32_t layers_ = 1;
  wgpu::Texture color_texture_ = nullptr;
  wgpu::TextureView color_view_ = nullptr;
  wgpu::Texture depth_texture_ = nullptr;
  wgpu::TextureView depth_array_view_ = nullptr;
  wgpu::TextureView depth_cube_view_ = nullptr;
  std::vector<wgpu::TextureView> layer_views_;
};

}
