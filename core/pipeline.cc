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
#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

/* The fragment stage of the present pass, see Graphics::PresentInternal.

   Every render target of this engine stores RGBA8Unorm, and the authored color
   values are already sRGB encoded (the source bitmaps come from SDL surfaces
   whose bytes are sRGB). The swapchain of the window, on the other hand, is
   usually an *Srgb format on the desktop, so the hardware applies the linear ->
   sRGB transfer function to whatever the fragment stage writes. Feeding it the
   already encoded bytes therefore encodes them a second time, which washes the
   picture out towards white.

   This stage undoes the encoding once -- it decodes the sampled (already
   encoded) texel back to linear -- so the encode the sRGB target performs
   cancels out and the pixel reaches the screen exactly as it was authored.
   The decode is the exact piecewise sRGB EOTF rather than the gamma 2.2
   approximation, so mid tones stay where the artists put them. */
static const char kFS_PresentBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 2, binding = 1) uniform sampler u_sampler;

layout(location = 0) out vec4 o_color;

vec3 srgb_to_linear(vec3 c) {
  vec3 lo = c / 12.92;
  vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
  return mix(lo, hi, step(vec3(0.04045), c));
}

void main() {
  vec4 color = texture(sampler2D(u_texture, u_sampler), v_texcoord) * v_color;
  o_color = vec4(srgb_to_linear(color.rgb), color.a);
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

/* A plane shows its bitmap tiled over the whole render target instead of the
   single copy of it a sprite reads, so the texture coordinate of a pixel runs
   over every tile: the vertex stage carries the coordinate of the plane's
   surface, which grows past one over a tile, and the fraction of it is what the
   sampler is asked for. The tint of a Color and a Tone attribute applies the
   way it does to a sprite. */
static const char kFS_PlaneBase[] = R"(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 2, binding = 1) uniform sampler u_sampler;

layout(set = 3, binding = 0) uniform PlaneParam {
  vec4 blend_color;
  vec4 blend_tone;
} u_plane;

layout(location = 0) out vec4 o_color;

vec4 apply_tint(vec4 color, vec4 blend_color, vec4 blend_tone) {
  float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
  vec4 result = vec4(mix(color.rgb, vec3(luminance), blend_tone.a), color.a);
  result = vec4(result.rgb + blend_tone.rgb * result.a, result.a);
  return vec4(mix(result.rgb, blend_color.rgb * result.a, blend_color.a), result.a);
}

void main() {
  vec2 texcoord = fract(v_texcoord);
  vec4 color = texture(sampler2D(u_texture, u_sampler), texcoord);
  o_color = apply_tint(color, u_plane.blend_color, u_plane.blend_tone) * v_color;
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

/*! The stencil state of a pass which marks the pixels it covers: the stencil
    test never rejects a fragment, the fragment replaces whatever the stencil
    held with the value a node marks its region with, and the stencil is
    written wherever the fragment lands. The depth of the attachment is unused
    by the engine, so the depth test always passes and writes nothing. */
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

/*! The stencil state of a pass which is clipped to the region another pass
    marked: a fragment passes only where the stencil carries the reference
    value the marking pass wrote, and the stencil is left alone. */
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

/*! The stencil state of a pipeline which draws into a pass that carries a
    depth-stencil attachment but does not care about it: the depth test always
    passes without writing and the stencil is read and written with a mask of
    zero, so the attachment is left exactly as it was.

    Every frame of this engine binds the depth-stencil texture of its render
    target, see Node::Render, and WebGPU requires a pipeline which is used in
    a pass to declare a state for every attachment of that pass. This is the
    state of every pipeline which is not one of the two stencil users above. */
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

/* ----- The blend state of a user authored Effect ----- */

namespace {

//! Removes the leading and trailing whitespace of a field.
std::string_view Trim(std::string_view text) {
  constexpr std::string_view kWhitespace = " \t\r\n";
  const size_t begin = text.find_first_not_of(kWhitespace);
  if (begin == std::string_view::npos)
    return {};
  const size_t end = text.find_last_not_of(kWhitespace);
  return text.substr(begin, end - begin + 1);
}

//! The ASCII lowercase copy of a name, so the keys and the values are matched
//! without regard for case.
std::string Lower(std::string_view text) {
  std::string lower(text);
  std::transform(
      lower.begin(), lower.end(), lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower;
}

//! One name of the blend grammar together with what it selects.
template <typename Ty>
struct BlendName {
  const char* name;
  Ty value;
};

//! The blend factors the "src"/"dst" style keys accept, in the spelling of the
//! GLSL/WebGPU factor each names.
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

//! The blend operations an "op" key accepts.
constexpr BlendName<wgpu::BlendOperation> kBlendOperations[] = {
    {"add", wgpu::BlendOperation::Add},
    {"subtract", wgpu::BlendOperation::Subtract},
    {"reverse-subtract", wgpu::BlendOperation::ReverseSubtract},
    {"min", wgpu::BlendOperation::Min},
    {"max", wgpu::BlendOperation::Max},
};

//! Reads a blend factor of the grammar, or throws when \p name is not one.
wgpu::BlendFactor ReadBlendFactor(const std::string& name) {
  for (const BlendName<wgpu::BlendFactor>& entry : kBlendFactors)
    if (name == entry.name)
      return entry.value;
  throw Exception(Exception::kRGSSError,
                  "effect: '{}' is not a blend factor name", name);
}

//! Reads a blend operation of the grammar, or throws when \p name is not one.
wgpu::BlendOperation ReadBlendOperation(const std::string& name) {
  for (const BlendName<wgpu::BlendOperation>& entry : kBlendOperations)
    if (name == entry.name)
      return entry.value;
  throw Exception(Exception::kRGSSError,
                  "effect: '{}' is not a blend operation name", name);
}

}  // namespace

wgpu::PrimitiveState GetDefaultPrimitiveState() {
  wgpu::PrimitiveState primitive;
  primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  return primitive;
}

std::optional<wgpu::BlendState> ParseBlendState(std::string_view states) {
  const std::string text = Lower(Trim(states));

  // An empty description, or one of the aliases of the default blend
  if (text.empty() || text == "normal" || text == "alpha" ||
      text == "premultiplied")
    return *GetBlendState(BLEND_NORMAL);
  if (text == "none" || text == "off" || text == "disable" ||
      text == "disabled")
    return std::nullopt;
  if (text == "addition" || text == "add" || text == "additive")
    return *GetBlendState(BLEND_ADDITION);
  if (text == "subtract" || text == "sub")
    return *GetBlendState(BLEND_SUBTRACT);

  /* A field list overrides the fields of the default blend, so an author only
     writes what differs from the plain premultiplied composite. */
  wgpu::BlendState state = *GetBlendState(BLEND_NORMAL);

  const std::string_view view(text);
  size_t position = 0;
  while (position < view.size()) {
    const size_t separator = view.find_first_of(";,", position);
    const std::string_view field =
        Trim(view.substr(position, separator == std::string_view::npos
                                       ? std::string_view::npos
                                       : separator - position));
    position =
        separator == std::string_view::npos ? view.size() : separator + 1;
    if (field.empty())
      continue;

    const size_t equal = field.find('=');
    if (equal == std::string_view::npos)
      throw Exception(Exception::kRGSSError,
                      "effect: '{}' is not a blend field of the form 'key=value'",
                      std::string(field));

    const std::string key = Lower(Trim(field.substr(0, equal)));
    const std::string value = Lower(Trim(field.substr(equal + 1)));

    if (key == "src_color" || key == "src_rgb" || key == "color_src" ||
        key == "src")
      state.color.srcFactor = ReadBlendFactor(value);
    else if (key == "dst_color" || key == "dst_rgb" || key == "color_dst" ||
             key == "dst")
      state.color.dstFactor = ReadBlendFactor(value);
    else if (key == "op_color" || key == "color_op" || key == "equal_rgb" ||
             key == "equal_color")
      state.color.operation = ReadBlendOperation(value);
    else if (key == "src_alpha" || key == "alpha_src")
      state.alpha.srcFactor = ReadBlendFactor(value);
    else if (key == "dst_alpha" || key == "alpha_dst")
      state.alpha.dstFactor = ReadBlendFactor(value);
    else if (key == "op_alpha" || key == "alpha_op" ||
             key == "equal_alpha")
      state.alpha.operation = ReadBlendOperation(value);
    else
      throw Exception(Exception::kRGSSError,
                      "effect: '{}' is not a blend field name", key);
  }

  return state;
}

/* ----- TextureBase ----- */

TextureBase::TextureBase()
    : Pipeline(kVS_TransformBase, kFS_TextureBase, {{0, 1, 2}}) {}

/* ----- PresentBase ----- */

/* The pipeline of the present pass, see Graphics::PresentInternal: the plain
   transform vertex stage and a fragment stage which cancels the extra sRGB
   encode of an *Srgb swapchain, see kFS_PresentBase. */
PresentBase::PresentBase()
    : Pipeline(kVS_TransformBase, kFS_PresentBase, {{0, 1, 2}}) {}

/*! The same shaders as TextureBase, but the object data of set 1 is staged in
    the pool of the frame and bound with a dynamic offset, which is what the
    nodes of a window read, see TextureBaseDynamic. */
TextureBaseDynamic::TextureBaseDynamic()
    : Pipeline(kVS_TransformBase, kFS_TextureBase, {{0, 1, 2}}, {1}) {}

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

/* ----- PlaneBase ----- */

/* A plane keeps its object transform in the pool of the frame and the two
   values of its tint in a buffer of its own: a scene holds a handful of planes
   and a plane is drawn with one quad, so there is nothing to batch and the
   object set is the only one which travels with a dynamic offset. */
PlaneBase::PlaneBase()
    : Pipeline(kVS_TransformBase, kFS_PlaneBase, {{0, 1, 2}}, {1}) {}

/* ----- TransitionAlpha ----- */

TransitionAlpha::TransitionAlpha()
    : Pipeline(kVS_TransformBase, kFS_TransitionAlpha, {{0, 1, 2}}) {}

/* ----- TransitionVague ----- */

TransitionVague::TransitionVague()
    : Pipeline(kVS_TransformBase, kFS_TransitionMap, {{0, 1, 2}}) {}

/* ----- ShaderSet ----- */

ShaderSet::ShaderSet() : shader() {
  // The built in pipelines and a user authored Effect share these two defaults
  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();
  wgpu::TextureFormat target = kColorTargetFormat;

  /* Every pass of this engine carries the depth-stencil texture of its
     render target, see Node::Render, so every pipeline has to declare a state
     for it: GetDepthStencilState() returns the state of a pipeline which
     ignores the attachment. */
  wgpu::DepthStencilState* depth_stencil = GetDepthStencilState();

  state.texture_noblend = shader.texture_base.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});
  state.texture_pma = shader.texture_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.color_noblend = shader.color_base.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});
  state.color_pma = shader.color_base.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  state.texture_dynamic_noblend = shader.texture_base_dynamic.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});
  state.texture_dynamic_pma = shader.texture_base_dynamic.MakeState(
      primitive, *depth_stencil,
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
  for (auto it : {BLEND_NONE, BLEND_NORMAL, BLEND_ADDITION, BLEND_SUBTRACT}) {
    state.tint_blends[it] = shader.tint_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
    state.sprite_blends[it] = shader.sprite_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
    state.plane_blends[it] = shader.plane_base.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
    state.geometry_blends[it] = shader.texture_base_dynamic.MakeState(
        primitive, *depth_stencil,
        {wgpu::ColorTargetState{.format = target, .blend = GetBlendState(it)}});
  }
  state.transition_alpha = shader.transition_alpha.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});
  state.transition_vague = shader.transition_vague.MakeState(
      primitive, *depth_stencil, {wgpu::ColorTargetState{.format = target}});

  state.texture_stencil_write = shader.texture_base_dynamic.MakeState(
      primitive, *GetStencilWriteState(),
      {wgpu::ColorTargetState{.format = target,
                              .writeMask = wgpu::ColorWriteMask::None}});
  state.texture_stencil_test = shader.texture_base_dynamic.MakeState(
      primitive, *GetStencilTestState(),
      {wgpu::ColorTargetState{.format = target,
                              .blend = GetBlendState(BLEND_NORMAL)}});
}

}  // namespace urge
