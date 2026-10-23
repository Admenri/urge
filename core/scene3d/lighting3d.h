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

#include "glm/glm.hpp"

#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/render/gpu_utils.h"

namespace urge {

inline constexpr uint32_t kMaxLights = 8;
inline constexpr uint32_t kMaxCascades = 4;

struct LightData {
  glm::vec4 position_type = glm::vec4(0.0f);
  glm::vec4 direction_range = glm::vec4(0.0f);
  glm::vec4 color_intensity = glm::vec4(0.0f);
};

struct ViewData {
  glm::mat4 view_proj = glm::mat4(1.0f);
  glm::mat4 view = glm::mat4(1.0f);
  glm::vec4 camera_position = glm::vec4(0.0f);
  glm::vec4 ambient = glm::vec4(0.0f);
  glm::mat4 light_view_proj = glm::mat4(1.0f);
  LightData lights[kMaxLights] = {};

  glm::vec4 cascade_splits = glm::vec4(0.0f);
  glm::vec4 cascade_params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  glm::mat4 cascade_matrices[kMaxCascades] = {};
  glm::vec4 camera_forward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

  glm::vec4 point_shadow_params = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
  glm::mat4 point_face_view_proj[6] = {};
};

struct MeshData {
  glm::mat4 model = glm::mat4(1.0f);
  glm::mat4 normal = glm::mat4(1.0f);
};

struct GlobalsData {
  glm::vec4 time = glm::vec4(0.0f);
  glm::vec4 viewport = glm::vec4(1.0f);
};

std::vector<util::LayoutEntry> ViewEntries();
std::vector<util::LayoutEntry> ShadowViewEntries();
std::vector<util::LayoutEntry> MeshEntries();
std::vector<util::LayoutEntry> MeshUniformEntries();
std::vector<util::LayoutEntry> MaterialEntries();
std::vector<util::LayoutEntry> GlobalsEntries();

wgpu::BindGroupLayout AcquireViewLayout();
wgpu::BindGroupLayout AcquireShadowViewLayout();
wgpu::BindGroupLayout AcquireMeshLayout();
wgpu::BindGroupLayout AcquireMeshUniformLayout();
wgpu::BindGroupLayout AcquireGlobalsLayout();

wgpu::TextureView AcquireEmptyShadowView();
wgpu::TextureView AcquireEmptyShadowCubeView();
wgpu::TextureView AcquireEmptyShadowArrayView();
wgpu::Sampler AcquireShadowSampler();
wgpu::Buffer AcquireDummyJointBuffer();

}
