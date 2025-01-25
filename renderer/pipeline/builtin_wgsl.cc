// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/builtin_wgsl.h"

namespace renderer {

const std::string kBaseRenderWGSL = R"(

struct WorldMatrix {
  projMat: mat4x4<f32>,
  transMat: mat4x4<f32>,
};

struct VertexOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) uv: vec2<f32>,
  @location(1) color: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_transform: WorldMatrix;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;
@group(1) @binding(2) var<uniform> u_texSize: vec2<f32>;

@vertex fn vertexMain(
    @location(0) pos: vec4<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>) -> VertexOutput {
  var result: VertexOutput;
  result.pos = u_transform.projMat * pos;
  result.pos = u_transform.transMat * result.pos;
  result.uv = u_texSize * uv;
  result.color = color;
  return result;
}

@fragment fn fragmentMain(vertex: VertexOutput) -> @location(0) vec4f {
  var tex = textureSample(u_texture, u_sampler, vertex.uv);
  tex.a *= vertex.color.a;
  return tex;
}

)";

const std::string kColorRenderWGSL = R"(

struct WorldMatrix {
  projMat: mat4x4<f32>,
  transMat: mat4x4<f32>,
};

struct VertexOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) color: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_transform: WorldMatrix;

@vertex fn vertexMain(
    @location(0) pos: vec4<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>) -> VertexOutput {
  var result: VertexOutput;
  result.pos = u_transform.projMat * pos;
  result.pos = u_transform.transMat * result.pos;
  result.color = color;
  return result;
}

@fragment fn fragmentMain(vertex: VertexOutput) -> @location(0) vec4f {
  return vertex.color;
}

)";

const std::string kViewportBaseRenderWGSL = R"(

struct WorldMatrix {
  projMat: mat4x4<f32>,
  transMat: mat4x4<f32>,
};

struct EffectParams {
  color: vec4<f32>,
  tone: vec4<f32>,
};

struct VertexOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

@group(0) @binding(0) var<uniform> u_transform: WorldMatrix;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;
@group(1) @binding(2) var<uniform> u_texSize: vec2<f32>;
@group(2) @binding(0) var<uniform> u_effect: EffectParams;

const lumaF: vec3<f32> = vec3<f32>(0.299, 0.587, 0.114);

@vertex fn vertexMain(
    @location(0) pos: vec4<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>) -> VertexOutput {
  var result: VertexOutput;
  result.pos = u_transform.projMat * pos;
  result.pos = u_transform.transMat * result.pos;
  result.uv = u_texSize * uv;
  return result;
}

@fragment fn fragmentMain(vertex: VertexOutput) -> @location(0) vec4f {
  var frag = textureSample(u_texture, u_sampler, vertex.uv);
  
  var luma: f32 = dot(frag.rgb, lumaF);
  frag.rgb = mix(frag.rgb, vec3<f32>(luma), u_effect.tone.w);
  frag.rgb = frag.rgb + u_effect.tone.rgb;
  
  frag.rgb = mix(frag.rgb, u_effect.color.rgb, u_effect.color.a);

  return frag;
}

)";

}  // namespace renderer
