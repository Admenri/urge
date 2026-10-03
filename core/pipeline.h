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
#include <optional>
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

//! The format of the single color target every pass of the engine carries.
constexpr wgpu::TextureFormat kColorTargetFormat =
    wgpu::TextureFormat::RGBA8Unorm;

/*! The primitive state of every draw the engine issues: a triangle list, with
    the default winding and no culling. */
wgpu::PrimitiveState GetDefaultPrimitiveState();

/*! The depth-stencil state of a pipeline which is used in a pass that carries
    a depth-stencil attachment but does not care about it: the depth test always
    passes without writing and the stencil is masked off. It is the state of
    every pipeline which is not one of the two stencil users of ShaderSet, see
    Node::Render. */
wgpu::DepthStencilState* GetDepthStencilState();

/*! Reads the blend state of a color target from the textual description a user
    authored Effect is built with, so the state does not have to be one of the
    premultiplied blends of BlendType.

    An empty \p states, or one of the preset names "normal", "alpha" and
    "premultiplied", selects the premultiplied alpha blend of BLEND_NORMAL, the
    blend a sprite composites with; "none" (also "off") selects no blending at
    all, which the empty optional of the result reports; "addition" and
    "subtract" select the state of the matching BlendType.

    Otherwise \p states is a ";" (or ",") separated list of "key=value" fields
    which start from the state of BLEND_NORMAL and override it. A key is one of
    "src_color"/"src_rgb", "dst_color"/"dst_rgb", "op_color"/"equal_rgb" and
    their "alpha" counterparts, and a value is a blend factor name ("zero",
    "one", "src", "one-minus-src", "src-alpha", "one-minus-src-alpha", "dst",
    "one-minus-dst", "dst-alpha", "one-minus-dst-alpha", "src-alpha-saturated",
    "constant", "one-minus-constant") or, for the "op" keys, a blend operation
    ("add", "subtract", "reverse-subtract", "min", "max").

    The name comparison ignores case and a key the engine does not know, or a
    value outside the domain of its key, raises an Exception. */
std::optional<wgpu::BlendState> ParseBlendState(std::string_view states);

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
