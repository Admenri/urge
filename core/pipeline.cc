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

#include "core/pipeline.h"

#include <algorithm>
#include <utility>

#include "core/exception.h"

namespace urge {

static const char kVS_TransformBase[] = R"(#version 450
layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec4 in_color;

layout(set = 0, binding = 0) uniform SceneData {
  mat4 view_proj_mat;
} u_scene;

layout(set = 1, binding = 0) uniform ObjectData {
  mat4 model_mat;
} u_object;

layout(location = 0) out vec2 v_texcoord;
layout(location = 1) out vec4 v_color;

void main() {
  gl_Position = u_scene.view_proj_mat * u_object.model_mat * in_position;
  v_texcoord = in_texcoord;
  v_color = in_color;
}
)";

static const char kFS_TextureBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 2, binding = 1) uniform sampler u_sampler;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(sampler2D(u_texture, u_sampler), v_texcoord) * v_color;
}
)";

static const char kFS_ColorBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = v_color;
}
)";

static const char kFS_TintBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 2, binding = 1) uniform sampler u_sampler;

layout(set = 3, binding = 0) uniform TintParam {
  vec4 blend_color;
  vec4 blend_tone;
} u_tint;

layout(location = 0) out vec4 o_color;

vec4 apply_tint(vec4 color, vec4 blend_color, vec4 blend_tone) {
  float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
  vec4 result = vec4(mix(color.rgb, vec3(luminance), blend_tone.a), color.a);
  result = vec4(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4(mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

void main() {
  vec4 color = texture(sampler2D(u_texture, u_sampler), v_texcoord);
  o_color = apply_tint(color, u_tint.blend_color, u_tint.blend_tone) * v_color;
}
)";

static const char kFS_SpriteBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 2, binding = 1) uniform sampler u_sampler;

layout(set = 3, binding = 0) uniform SpriteParam {
  vec4 blend_color;
  vec4 blend_tone;
  float bush_depth;
  float bush_opacity;
} u_sprite;

layout(location = 0) out vec4 o_color;

vec4 apply_tint(vec4 color, vec4 blend_color, vec4 blend_tone) {
  float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
  vec4 result = vec4(mix(color.rgb, vec3(luminance), blend_tone.a), color.a);
  result = vec4(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4(mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

void main() {
  vec4 color = texture(sampler2D(u_texture, u_sampler), v_texcoord);
  vec4 result = apply_tint(color, u_sprite.blend_color, u_sprite.blend_tone);

  float under_bush = v_texcoord.y > u_sprite.bush_depth ? 0.0 : 1.0;
  result *= clamp(u_sprite.bush_opacity + under_bush, 0.0, 1.0);

  o_color = result * v_color;
}
)";

static const char kFS_TransitionAlpha[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_frozen_scene;
layout(set = 2, binding = 1) uniform sampler u_frozen_sampler;
layout(set = 2, binding = 2) uniform texture2D u_current_scene;
layout(set = 2, binding = 3) uniform sampler u_current_sampler;

layout(location = 0) out vec4 o_color;

void main() {
  vec4 frozen = texture(sampler2D(u_frozen_scene, u_frozen_sampler), v_texcoord);
  vec4 current = texture(sampler2D(u_current_scene, u_current_sampler), v_texcoord);
  float progress = v_color.a;
  o_color = vec4(mix(frozen.rgb, current.rgb, progress), 1.0);
}
)";

static const char kFS_TransitionMap[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_frozen_scene;
layout(set = 2, binding = 1) uniform sampler u_frozen_sampler;
layout(set = 2, binding = 2) uniform texture2D u_current_scene;
layout(set = 2, binding = 3) uniform sampler u_current_sampler;
layout(set = 2, binding = 4) uniform texture2D u_mapping_texture;
layout(set = 2, binding = 5) uniform sampler u_mapping_sampler;

layout(location = 0) out vec4 o_color;

void main() {
  vec4 frozen = texture(sampler2D(u_frozen_scene, u_frozen_sampler), v_texcoord);
  vec4 current = texture(sampler2D(u_current_scene, u_current_sampler), v_texcoord);
  float mapped = texture(sampler2D(u_mapping_texture, u_mapping_sampler), v_texcoord).r;

  float vague = v_color.r;
  float progress = v_color.a;
  float threshold = clamp(mapped, progress, progress + vague);

  o_color = vec4(mix(frozen.rgb, current.rgb, 1.0 - (threshold - progress) / vague), 1.0);
}
)";

wgpu::BlendState* GetBlendState(BlendType type) {
  // BLEND_NORMAL: premultiplied alpha, src + dst * (1 - srcAlpha)
  static wgpu::BlendState normal{
      .color = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha},
      .alpha = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha}};
  // BLEND_ADDITION: src + dst
  static wgpu::BlendState addition{
      .color = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One},
      .alpha = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One}};
  // BLEND_SUBTRACT: dst - src
  static wgpu::BlendState subtract{
      .color = {.operation = wgpu::BlendOperation::ReverseSubtract,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One},
      .alpha = {.operation = wgpu::BlendOperation::ReverseSubtract,
                .srcFactor = wgpu::BlendFactor::Zero,
                .dstFactor = wgpu::BlendFactor::One}};

  switch (type) {
    default:
    case BLEND_NONE:
      return nullptr;
    case BLEND_NORMAL:
      return &normal;
    case BLEND_ADDITION:
      return &addition;
    case BLEND_SUBTRACT:
      return &subtract;
  }
}

/* ----- TextureBase ----- */

TextureBase::TextureBase()
    : Pipeline(kVS_TransformBase, kFS_TextureBase, {{0, 1, 2}}) {}

/* ----- ColorBase ----- */

ColorBase::ColorBase()
    : Pipeline(kVS_TransformBase, kFS_ColorBase, {{0, 1, 2}}) {}

/* ----- TintBase ----- */

/* The object transform of a tinted draw is uploaded in bulk, so the set which
   holds ObjectData is bound with a dynamic offset. */
TintBase::TintBase()
    : Pipeline(kVS_TransformBase, kFS_TintBase, {{0, 1, 2}}, {1}) {}

/* ----- SpriteBase ----- */

/* The object transform and the sprite parameter of every sprite of a frame are
   staged in one buffer each, so both sets are bound with a dynamic offset: the
   parameter is what a batch of sprites reads per sprite out of the same buffer,
   see UniformManager. */
SpriteBase::SpriteBase()
    : Pipeline(kVS_TransformBase, kFS_SpriteBase, {{0, 1, 2}}, {1, 3}) {}

/* ----- TransitionAlpha ----- */

TransitionAlpha::TransitionAlpha()
    : Pipeline(kVS_TransformBase, kFS_TransitionAlpha, {{0, 1, 2}}) {}

/* ----- TransitionVague ----- */

TransitionVague::TransitionVague()
    : Pipeline(kVS_TransformBase, kFS_TransitionMap, {{0, 1, 2}}) {}

/* ----- ShaderSet ----- */

ShaderSet::ShaderSet() : shader() {
  wgpu::PrimitiveState primitive;
  primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  wgpu::TextureFormat target = wgpu::TextureFormat::RGBA8Unorm;

  state.texture_noblend = shader.texture_base.MakeState(
      primitive, std::nullopt, {wgpu::ColorTargetState{.format = target}});
  state.texture_pma = shader.texture_base.MakeState(
      primitive, std::nullopt,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.color_noblend = shader.color_base.MakeState(
      primitive, std::nullopt, {wgpu::ColorTargetState{.format = target}});
  state.color_pma = shader.color_base.MakeState(
      primitive, std::nullopt,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  for (auto it : {BLEND_NONE, BLEND_NORMAL, BLEND_ADDITION, BLEND_SUBTRACT}) {
    state.tint_blends[it] = shader.tint_base.MakeState(
        primitive, std::nullopt,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
    state.sprite_blends[it] = shader.sprite_base.MakeState(
        primitive, std::nullopt,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
  }
  state.transition_alpha = shader.transition_alpha.MakeState(
      primitive, std::nullopt, {wgpu::ColorTargetState{.format = target}});
  state.transition_vague = shader.transition_vague.MakeState(
      primitive, std::nullopt, {wgpu::ColorTargetState{.format = target}});
}

}  // namespace urge
