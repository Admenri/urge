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

#include <array>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "core/common.h"
#include "core/gpu.h"
#include "core/object.h"
#include "core/shader.h"

namespace urge {

struct SceneData {
  glm::mat4 view_proj_mat;
};

struct ObjectData {
  glm::mat4 model_mat;
};

enum BlendType : int32_t {
  BLEND_NONE = -1,
  BLEND_NORMAL = 0,
  BLEND_ADDITION,
  BLEND_SUBTRACT,
};

wgpu::BlendState* GetBlendState(BlendType type);

class TextureBase : public Pipeline {
 public:
  TextureBase();
};

/*! The pipeline of the present pass, see Graphics::PresentInternal. It shares
    the transform vertex stage of TextureBase but decodes the sampled texel from
    sRGB back to linear, so the encode of an *Srgb swapchain does not apply a
    second time and the picture reaches the screen unwashed. */
class PresentBase : public Pipeline {
 public:
  PresentBase();
};

/*! The plain texture pipeline of the nodes whose object transform is staged in
    the pool of the frame and bound with a dynamic offset: WindowVX and WindowXP
    draw every part of a window -- the frame, the cursor, the contents -- out of
    one slot of that pool, so their object data travels with a dynamic offset
    while the object data of the screen root and of a bitmap does not, see
    TextureBase.

    The shaders of this pipeline are the ones of TextureBase; only the layout of
    set 1 differs, see Pipeline. */
class TextureBaseDynamic : public Pipeline {
 public:
  TextureBaseDynamic();
};

class ColorBase : public Pipeline {
 public:
  ColorBase();
};

class TintBase : public Pipeline {
 public:
  struct TintParam {
    glm::vec4 blend_color;
    glm::vec4 blend_tone;
  };

  TintBase();
};

class SpriteBase : public Pipeline {
 public:
  struct alignas(16) SpriteParam {
    glm::vec4 blend_color;
    glm::vec4 blend_tone;
    float bush_depth;
    float bush_opacity;
    float padding[2];
  };

  SpriteBase();
};

class PlaneBase : public Pipeline {
 public:
  struct alignas(16) PlaneParam {
    glm::vec4 blend_color;
    glm::vec4 blend_tone;
  };

  PlaneBase();
};

class TransitionAlpha : public Pipeline {
 public:
  TransitionAlpha();
};

class TransitionVague : public Pipeline {
 public:
  TransitionVague();
};

struct ShaderSet : public Singleton<ShaderSet> {
  struct {
    TextureBase texture_base;
    PresentBase present_base;
    TextureBaseDynamic texture_base_dynamic;
    ColorBase color_base;
    TintBase tint_base;
    SpriteBase sprite_base;
    PlaneBase plane_base;
    TransitionAlpha transition_alpha;
    TransitionVague transition_vague;
  } shader;

  struct {
    wgpu::RenderPipeline texture_noblend;
    wgpu::RenderPipeline texture_pma;
    wgpu::RenderPipeline present;
    wgpu::RenderPipeline color_noblend;
    wgpu::RenderPipeline color_pma;
    std::map<BlendType, wgpu::RenderPipeline> tint_blends;
    std::map<BlendType, wgpu::RenderPipeline> sprite_blends;
    std::map<BlendType, wgpu::RenderPipeline> plane_blends;
    std::map<BlendType, wgpu::RenderPipeline> geometry_blends;
    wgpu::RenderPipeline transition_alpha;
    wgpu::RenderPipeline transition_vague;
    wgpu::RenderPipeline texture_dynamic_noblend;
    wgpu::RenderPipeline texture_dynamic_pma;
    wgpu::RenderPipeline texture_stencil_write;
    wgpu::RenderPipeline texture_stencil_test;
  } state;

  ShaderSet();
};

}  // namespace urge
