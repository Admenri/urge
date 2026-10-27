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

#include "core/render/pipeline.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/common/exception.h"

namespace urge {

namespace {

util::LayoutEntry Uniform(uint32_t binding,
                          wgpu::ShaderStage stage,
                          uint64_t size) {
  return {binding, stage, util::BufferLayout::Uniform(false, size)};
}

util::LayoutEntry ReadOnlyStorage(uint32_t binding,
                                  wgpu::ShaderStage stage,
                                  uint64_t size) {
  return {binding, stage, util::BufferLayout::ReadOnlyStorage(false, size)};
}

util::LayoutEntry Texture(uint32_t binding, wgpu::ShaderStage stage) {
  return {binding, stage,
          util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)};
}

util::LayoutEntry Sampler(uint32_t binding, wgpu::ShaderStage stage) {
  return {binding, stage, util::SamplerLayout::Filtering()};
}

const ShaderSource kVS_TransformBase = {
    R"(
struct SceneData {
  view_proj_mat: mat4x4<f32>,
};

struct ObjectData {
  model_mat: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;
@group(1) @binding(0) var<uniform> u_object: ObjectData;

struct VertexInput {
  @location(0) in_position: vec4<f32>,
  @location(1) in_texcoord: vec2<f32>,
  @location(2) in_color: vec4<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
  var output: VertexOutput;
  output.position =
      u_scene.view_proj_mat * u_object.model_mat * input.in_position;
  output.v_texcoord = input.in_texcoord;
  output.v_color = input.in_color;
  return output;
}
)",
    "vs_main",
    {
        {0, {Uniform(0, wgpu::ShaderStage::Vertex, sizeof(SceneData))}},
        {1, {Uniform(0, wgpu::ShaderStage::Vertex, sizeof(ObjectData))}},
    },
};

const ShaderSource kFS_TextureBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  return textureSample(u_texture, u_sampler, input.v_texcoord) * input.v_color;
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_TilemapBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let color = textureSample(u_texture, u_sampler, input.v_texcoord);
  let flashed = mix(color.rgb, input.v_color.rgb * color.a, input.v_color.a);
  return vec4<f32>(flashed, color.a);
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_TextureMaskBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

@group(3) @binding(0) var u_mask_texture: texture_2d<f32>;
@group(3) @binding(1) var u_mask_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let color = textureSample(u_texture, u_sampler, input.v_texcoord);
  let mask_alpha =
      textureSample(u_mask_texture, u_mask_sampler, input.v_texcoord).a;
  return color * mask_alpha * input.v_color;
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
        {3,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_PresentBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

fn srgb_to_linear(c: vec3<f32>) -> vec3<f32> {
  let lo = c / 12.92;
  let hi = pow((c + vec3<f32>(0.055)) / 1.055, vec3<f32>(2.4));
  return mix(lo, hi, step(vec3<f32>(0.04045), c));
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let color = textureSample(u_texture, u_sampler, input.v_texcoord) * input.v_color;
  return vec4<f32>(srgb_to_linear(color.rgb), color.a);
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_ColorBase = {
    R"(
struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  return input.v_color;
}
)",
    "fs_main",
    {},
};

const ShaderSource kFS_TintBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct TintParam {
  blend_color: vec4<f32>,
  blend_tone: vec4<f32>,
};

@group(3) @binding(0) var<uniform> u_tint: TintParam;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

fn apply_tint(color: vec4<f32>,
              blend_color: vec4<f32>,
              blend_tone: vec4<f32>) -> vec4<f32> {
  let luminance = dot(color.rgb, vec3<f32>(0.299, 0.587, 0.114));
  var result =
      vec4<f32>(mix(color.rgb, vec3<f32>(luminance), blend_tone.a), color.a);
  result = vec4<f32>(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4<f32>(
      mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let color = textureSample(u_texture, u_sampler, input.v_texcoord);
  return apply_tint(color, u_tint.blend_color, u_tint.blend_tone) * input.v_color;
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
        {3,
         {Uniform(0,
                  wgpu::ShaderStage::Fragment,
                  sizeof(TintBase::TintParam))}},
    },
};

const ShaderSource kVS_SpriteBatch = {
    R"(
struct SceneData {
  view_proj_mat: mat4x4<f32>,
};

struct SpriteParam {
  model_mat: mat4x4<f32>,
  blend_color: vec4<f32>,
  blend_tone: vec4<f32>,
  bush_depth: f32,
  bush_opacity: f32,
};

struct SpriteParamArray {
  params: array<SpriteParam>,
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;
@group(1) @binding(0) var<storage, read> u_sprite: SpriteParamArray;

struct VertexInput {
  @builtin(vertex_index) vertex_index: u32,
  @location(0) in_position: vec4<f32>,
  @location(1) in_texcoord: vec2<f32>,
  @location(2) in_color: vec4<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
  @location(2) @interpolate(flat) v_blend_color: vec4<f32>,
  @location(3) @interpolate(flat) v_blend_tone: vec4<f32>,
  @location(4) @interpolate(flat) v_bush_depth: f32,
  @location(5) @interpolate(flat) v_bush_opacity: f32,
};

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
  // Six vertices per sprite, so the index is a flat divide.
  let param = u_sprite.params[input.vertex_index / 6u];

  var output: VertexOutput;
  output.position = u_scene.view_proj_mat * param.model_mat * input.in_position;
  output.v_texcoord = input.in_texcoord;
  output.v_color = input.in_color;
  output.v_blend_color = param.blend_color;
  output.v_blend_tone = param.blend_tone;
  output.v_bush_depth = param.bush_depth;
  output.v_bush_opacity = param.bush_opacity;
  return output;
}
)",
    "vs_main",
    {
        {0, {Uniform(0, wgpu::ShaderStage::Vertex, sizeof(SceneData))}},
        {1, {ReadOnlyStorage(0, wgpu::ShaderStage::Vertex, 0)}},
    },
};

const ShaderSource kFS_SpriteBatch = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
  @location(2) @interpolate(flat) v_blend_color: vec4<f32>,
  @location(3) @interpolate(flat) v_blend_tone: vec4<f32>,
  @location(4) @interpolate(flat) v_bush_depth: f32,
  @location(5) @interpolate(flat) v_bush_opacity: f32,
};

fn apply_tint(color: vec4<f32>,
              blend_color: vec4<f32>,
              blend_tone: vec4<f32>) -> vec4<f32> {
  let luminance = dot(color.rgb, vec3<f32>(0.299, 0.587, 0.114));
  var result =
      vec4<f32>(mix(color.rgb, vec3<f32>(luminance), blend_tone.a), color.a);
  result = vec4<f32>(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4<f32>(
      mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let color = textureSample(u_texture, u_sampler, input.v_texcoord);
  var result = apply_tint(color, input.v_blend_color, input.v_blend_tone);

  // `bush_depth` is a v coordinate, so the *top* of the sprite is the small
  // y: above the bush line the sprite is drawn whole, below it `bush_opacity`
  // takes over.  Written as an `if` rather than `select` on purpose --
  // `select(f, t, cond)` yields the *second* argument when cond is true, the
  // opposite order from GLSL's `cond ? a : b`, and getting that backwards
  // turns every sprite into a half transparent one (the engine's default
  // bush opacity is 128/255).
  var under_bush = 1.0;
  if (input.v_texcoord.y > input.v_bush_depth) {
    under_bush = 0.0;
  }
  result = result * clamp(input.v_bush_opacity + under_bush, 0.0, 1.0);

  return result * input.v_color;
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_PlaneBase = {
    R"(
@group(2) @binding(0) var u_texture: texture_2d<f32>;
@group(2) @binding(1) var u_sampler: sampler;

struct PlaneParam {
  blend_color: vec4<f32>,
  blend_tone: vec4<f32>,
};

@group(3) @binding(0) var<uniform> u_plane: PlaneParam;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

fn apply_tint(color: vec4<f32>,
              blend_color: vec4<f32>,
              blend_tone: vec4<f32>) -> vec4<f32> {
  let luminance = dot(color.rgb, vec3<f32>(0.299, 0.587, 0.114));
  var result =
      vec4<f32>(mix(color.rgb, vec3<f32>(luminance), blend_tone.a), color.a);
  result = vec4<f32>(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4<f32>(
      mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let texcoord = fract(input.v_texcoord);
  let color = textureSample(u_texture, u_sampler, texcoord);
  return apply_tint(color, u_plane.blend_color, u_plane.blend_tone) *
         input.v_color;
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment)}},
        {3,
         {Uniform(0,
                  wgpu::ShaderStage::Fragment,
                  sizeof(PlaneBase::PlaneParam))}},
    },
};

const ShaderSource kFS_TransitionAlpha = {
    R"(
@group(2) @binding(0) var u_frozen_scene: texture_2d<f32>;
@group(2) @binding(1) var u_frozen_sampler: sampler;
@group(2) @binding(2) var u_current_scene: texture_2d<f32>;
@group(2) @binding(3) var u_current_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let frozen =
      textureSample(u_frozen_scene, u_frozen_sampler, input.v_texcoord);
  let current =
      textureSample(u_current_scene, u_current_sampler, input.v_texcoord);
  let progress = input.v_color.a;
  return vec4<f32>(mix(frozen.rgb, current.rgb, progress), 1.0);
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment),
          Texture(2, wgpu::ShaderStage::Fragment),
          Sampler(3, wgpu::ShaderStage::Fragment)}},
    },
};

const ShaderSource kFS_TransitionMap = {
    R"(
@group(2) @binding(0) var u_frozen_scene: texture_2d<f32>;
@group(2) @binding(1) var u_frozen_sampler: sampler;
@group(2) @binding(2) var u_current_scene: texture_2d<f32>;
@group(2) @binding(3) var u_current_sampler: sampler;
@group(2) @binding(4) var u_mapping_texture: texture_2d<f32>;
@group(2) @binding(5) var u_mapping_sampler: sampler;

struct FragmentInput {
  @location(0) v_texcoord: vec2<f32>,
  @location(1) v_color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let frozen =
      textureSample(u_frozen_scene, u_frozen_sampler, input.v_texcoord);
  let current =
      textureSample(u_current_scene, u_current_sampler, input.v_texcoord);
  let mapped =
      textureSample(u_mapping_texture, u_mapping_sampler, input.v_texcoord).r;

  let vague = input.v_color.r;
  let progress = input.v_color.a;
  let threshold = clamp(mapped, progress, progress + vague);

  return vec4<f32>(
      mix(frozen.rgb, current.rgb, 1.0 - (threshold - progress) / vague), 1.0);
}
)",
    "fs_main",
    {
        {2,
         {Texture(0, wgpu::ShaderStage::Fragment),
          Sampler(1, wgpu::ShaderStage::Fragment),
          Texture(2, wgpu::ShaderStage::Fragment),
          Sampler(3, wgpu::ShaderStage::Fragment),
          Texture(4, wgpu::ShaderStage::Fragment),
          Sampler(5, wgpu::ShaderStage::Fragment)}},
    },
};

wgpu::DepthStencilState* GetStencilWriteState() {
  static wgpu::DepthStencilState state{
      .format = wgpu::TextureFormat::Depth24PlusStencil8,
      .depthWriteEnabled = false,
      .depthCompare = wgpu::CompareFunction::Always,
      .stencilFront = {.compare = wgpu::CompareFunction::Always,
                       .failOp = wgpu::StencilOperation::Keep,
                       .depthFailOp = wgpu::StencilOperation::Keep,
                       .passOp = wgpu::StencilOperation::Replace},
      .stencilBack = {.compare = wgpu::CompareFunction::Always,
                      .failOp = wgpu::StencilOperation::Keep,
                      .depthFailOp = wgpu::StencilOperation::Keep,
                      .passOp = wgpu::StencilOperation::Replace},
      .stencilReadMask = 0xFF,
      .stencilWriteMask = 0xFF};
  return &state;
}

wgpu::DepthStencilState* GetStencilTestState() {
  static wgpu::DepthStencilState state{
      .format = wgpu::TextureFormat::Depth24PlusStencil8,
      .depthWriteEnabled = false,
      .depthCompare = wgpu::CompareFunction::Always,
      .stencilFront = {.compare = wgpu::CompareFunction::Equal,
                       .failOp = wgpu::StencilOperation::Keep,
                       .depthFailOp = wgpu::StencilOperation::Keep,
                       .passOp = wgpu::StencilOperation::Keep},
      .stencilBack = {.compare = wgpu::CompareFunction::Equal,
                      .failOp = wgpu::StencilOperation::Keep,
                      .depthFailOp = wgpu::StencilOperation::Keep,
                      .passOp = wgpu::StencilOperation::Keep},
      .stencilReadMask = 0xFF,
      .stencilWriteMask = 0x00};
  return &state;
}

std::string_view Trim(std::string_view text) {
  constexpr std::string_view kWhitespace = " \t\r\n";
  const size_t begin = text.find_first_not_of(kWhitespace);
  if (begin == std::string_view::npos)
    return {};
  const size_t end = text.find_last_not_of(kWhitespace);
  return text.substr(begin, end - begin + 1);
}

std::string Lower(std::string_view text) {
  std::string lower(text);
  std::transform(
      lower.begin(), lower.end(), lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower;
}

template <typename Ty>
struct BlendName {
  const char* name;
  Ty value;
};

constexpr BlendName<wgpu::BlendFactor> kBlendFactors[] = {
    {"zero", wgpu::BlendFactor::Zero},
    {"one", wgpu::BlendFactor::One},
    {"src", wgpu::BlendFactor::Src},
    {"one-minus-src", wgpu::BlendFactor::OneMinusSrc},
    {"src-alpha", wgpu::BlendFactor::SrcAlpha},
    {"one-minus-src-alpha", wgpu::BlendFactor::OneMinusSrcAlpha},
    {"dst", wgpu::BlendFactor::Dst},
    {"one-minus-dst", wgpu::BlendFactor::OneMinusDst},
    {"dst-alpha", wgpu::BlendFactor::DstAlpha},
    {"one-minus-dst-alpha", wgpu::BlendFactor::OneMinusDstAlpha},
    {"src-alpha-saturated", wgpu::BlendFactor::SrcAlphaSaturated},
    {"constant", wgpu::BlendFactor::Constant},
    {"one-minus-constant", wgpu::BlendFactor::OneMinusConstant},
    {"src1", wgpu::BlendFactor::Src1},
    {"one-minus-src1", wgpu::BlendFactor::OneMinusSrc1},
    {"src1-alpha", wgpu::BlendFactor::Src1Alpha},
    {"one-minus-src1-alpha", wgpu::BlendFactor::OneMinusSrc1Alpha},
};

constexpr BlendName<wgpu::BlendOperation> kBlendOperations[] = {
    {"add", wgpu::BlendOperation::Add},
    {"subtract", wgpu::BlendOperation::Subtract},
    {"reverse-subtract", wgpu::BlendOperation::ReverseSubtract},
    {"min", wgpu::BlendOperation::Min},
    {"max", wgpu::BlendOperation::Max},
};

wgpu::BlendFactor ReadBlendFactor(URGE_EXCEPT, const std::string& name) {
  for (const BlendName<wgpu::BlendFactor>& entry : kBlendFactors)
    if (name == entry.name)
      return entry.value;
  exception_state.Throw(ExceptionState::kRGSSError,
                        "effect: '{}' is not a blend factor name", name);
  return {};
}

wgpu::BlendOperation ReadBlendOperation(URGE_EXCEPT, const std::string& name) {
  for (const BlendName<wgpu::BlendOperation>& entry : kBlendOperations)
    if (name == entry.name)
      return entry.value;
  exception_state.Throw(ExceptionState::kRGSSError,
                        "effect: '{}' is not a blend operation name", name);
  return {};
}

}

wgpu::BlendState* GetBlendState(BlendType type) {
  static wgpu::BlendState normal{
      .color = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha},
      .alpha = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha}};
  static wgpu::BlendState addition{
      .color = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One},
      .alpha = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One}};
  static wgpu::BlendState alpha{
      .color = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha},
      .alpha = {.operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha}};
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
    case BLEND_ALPHA:
      return &alpha;
  }
}

wgpu::PrimitiveState GetDefaultPrimitiveState() {
  wgpu::PrimitiveState primitive;
  primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  return primitive;
}

wgpu::DepthStencilState* GetDepthStencilState() {
  static wgpu::DepthStencilState state{
      .format = wgpu::TextureFormat::Depth24PlusStencil8,
      .depthWriteEnabled = false,
      .depthCompare = wgpu::CompareFunction::Always,
      .stencilFront = {.compare = wgpu::CompareFunction::Always,
                       .failOp = wgpu::StencilOperation::Keep,
                       .depthFailOp = wgpu::StencilOperation::Keep,
                       .passOp = wgpu::StencilOperation::Keep},
      .stencilBack = {.compare = wgpu::CompareFunction::Always,
                      .failOp = wgpu::StencilOperation::Keep,
                      .depthFailOp = wgpu::StencilOperation::Keep,
                      .passOp = wgpu::StencilOperation::Keep},
      .stencilReadMask = 0x00,
      .stencilWriteMask = 0x00};
  return &state;
}

wgpu::DepthStencilState* GetDepthStencilTestState() {
  static wgpu::DepthStencilState state{
      .format = wgpu::TextureFormat::Depth24PlusStencil8,
      .depthWriteEnabled = true,
      .depthCompare = wgpu::CompareFunction::LessEqual,
      .stencilFront = {.compare = wgpu::CompareFunction::Always,
                       .failOp = wgpu::StencilOperation::Keep,
                       .depthFailOp = wgpu::StencilOperation::Keep,
                       .passOp = wgpu::StencilOperation::Keep},
      .stencilBack = {.compare = wgpu::CompareFunction::Always,
                      .failOp = wgpu::StencilOperation::Keep,
                      .depthFailOp = wgpu::StencilOperation::Keep,
                      .passOp = wgpu::StencilOperation::Keep},
      .stencilReadMask = 0x00,
      .stencilWriteMask = 0x00};
  return &state;
}

std::optional<wgpu::BlendState> ParseBlendState(URGE_EXCEPT,
                                                std::string_view states) {
  const std::string text = Lower(Trim(states));

  if (text.empty() || text == "normal" || text == "premultiplied")
    return *GetBlendState(BLEND_NORMAL);
  if (text == "alpha" || text == "straight" || text == "translucent")
    return *GetBlendState(BLEND_ALPHA);
  if (text == "none" || text == "off" || text == "disable" ||
      text == "disabled")
    return std::nullopt;
  if (text == "addition" || text == "add" || text == "additive")
    return *GetBlendState(BLEND_ADDITION);
  if (text == "subtract" || text == "sub")
    return *GetBlendState(BLEND_SUBTRACT);

  wgpu::BlendState state = *GetBlendState(BLEND_NORMAL);

  const std::string_view view(text);
  size_t position = 0;
  while (position < view.size()) {
    const size_t separator = view.find_first_of(";,", position);
    const std::string_view field = Trim(view.substr(
        position, separator == std::string_view::npos ? std::string_view::npos
                                                      : separator - position));
    position =
        separator == std::string_view::npos ? view.size() : separator + 1;
    if (field.empty())
      continue;

    const size_t equal = field.find('=');
    if (equal == std::string_view::npos) {
      exception_state.Throw(
          ExceptionState::kRGSSError,
          "effect: '{}' is not a blend field of the form 'key=value'",
          std::string(field));
      return std::nullopt;
    }

    const std::string key = Lower(Trim(field.substr(0, equal)));
    const std::string value = Lower(Trim(field.substr(equal + 1)));

    if (key == "src_color" || key == "src_rgb" || key == "color_src" ||
        key == "src")
      state.color.srcFactor = ReadBlendFactor(exception_state, value);
    else if (key == "dst_color" || key == "dst_rgb" || key == "color_dst" ||
             key == "dst")
      state.color.dstFactor = ReadBlendFactor(exception_state, value);
    else if (key == "op_color" || key == "color_op" || key == "equal_rgb" ||
             key == "equal_color")
      state.color.operation = ReadBlendOperation(exception_state, value);
    else if (key == "src_alpha" || key == "alpha_src")
      state.alpha.srcFactor = ReadBlendFactor(exception_state, value);
    else if (key == "dst_alpha" || key == "alpha_dst")
      state.alpha.dstFactor = ReadBlendFactor(exception_state, value);
    else if (key == "op_alpha" || key == "alpha_op" || key == "equal_alpha")
      state.alpha.operation = ReadBlendOperation(exception_state, value);
    else {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "effect: '{}' is not a blend field name", key);
      return std::nullopt;
    }
  }

  return state;
}

TextureBase::TextureBase()
    : Pipeline(kVS_TransformBase, kFS_TextureBase, QuadVertexLayout()) {}

TextureMaskBase::TextureMaskBase()
    : Pipeline(kVS_TransformBase, kFS_TextureMaskBase, QuadVertexLayout()) {}

PresentBase::PresentBase()
    : Pipeline(kVS_TransformBase, kFS_PresentBase, QuadVertexLayout()) {}

TextureBaseDynamic::TextureBaseDynamic()
    : Pipeline(kVS_TransformBase, kFS_TextureBase, QuadVertexLayout(), {1}) {}

ColorBase::ColorBase()
    : Pipeline(kVS_TransformBase, kFS_ColorBase, QuadVertexLayout()) {}

TintBase::TintBase()
    : Pipeline(kVS_TransformBase, kFS_TintBase, QuadVertexLayout(), {1}) {}

SpriteBase::SpriteBase()
    : Pipeline(kVS_SpriteBatch, kFS_SpriteBatch, QuadVertexLayout()) {}

PlaneBase::PlaneBase()
    : Pipeline(kVS_TransformBase, kFS_PlaneBase, QuadVertexLayout(), {1}) {}

TilemapBase::TilemapBase()
    : Pipeline(kVS_TransformBase, kFS_TilemapBase, QuadVertexLayout(), {1}) {}

TransitionAlpha::TransitionAlpha()
    : Pipeline(kVS_TransformBase, kFS_TransitionAlpha, QuadVertexLayout()) {}

TransitionVague::TransitionVague()
    : Pipeline(kVS_TransformBase, kFS_TransitionMap, QuadVertexLayout()) {}

ShaderSet::ShaderSet() : shader() {
  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();
  wgpu::TextureFormat target = kColorTargetFormat;
  wgpu::DepthStencilState* depth_stencil = GetDepthStencilState();

  const std::vector<BlendType> blend_types = {BLEND_NONE, BLEND_NORMAL,
                                              BLEND_ADDITION, BLEND_SUBTRACT};

  state.bitmap.texture_pma = shader.texture_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.bitmap.texture_mask_pma = shader.texture_mask_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.bitmap.color_noblend = shader.color_base.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});

  state.graphics.color_pma = shader.color_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.graphics.transition_alpha = shader.transition_alpha.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});
  state.graphics.transition_vague = shader.transition_vague.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});

  for (BlendType type : blend_types)
    state.geometry.geometry_blends[type] =
        shader.texture_base_dynamic.MakeState(
            primitive, *depth_stencil,
            {wgpu::ColorTargetState{.format = target,
                                    .blend = GetBlendState(type)}});

  for (BlendType type : blend_types)
    state.viewport.tint_blends[type] = shader.tint_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target,
                                .blend = GetBlendState(type)}});

  for (BlendType type : blend_types)
    state.window.tint_blends[type] = shader.tint_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target,
                                .blend = GetBlendState(type)}});
  state.window.texture_dynamic_pma = shader.texture_base_dynamic.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.window.texture_stencil_write = shader.texture_base_dynamic.MakeState(
      primitive, *GetStencilWriteState(),
      {wgpu::ColorTargetState{.format = target,
                              .writeMask = wgpu::ColorWriteMask::None}});
  state.window.texture_stencil_test = shader.texture_base_dynamic.MakeState(
      primitive, *GetStencilTestState(),
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});

  for (BlendType type : blend_types)
    state.sprite.sprite_blends[type] = shader.sprite_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target,
                                .blend = GetBlendState(type)}});

  for (BlendType type : blend_types)
    state.plane.plane_blends[type] = shader.plane_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target,
                                .blend = GetBlendState(type)}});

  state.tilemap.texture_dynamic_pma = shader.texture_base_dynamic.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});

  state.tilemap.texture_flash = shader.tilemap_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
}

}
