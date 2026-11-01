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

#include "core/graphics/upscaler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/shader.h"

namespace urge {

namespace {

const ShaderSource kVS_Fullscreen = {
    R"(
struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
  var positions = array<vec2<f32>, 3>(
      vec2<f32>(-1.0, -1.0),
      vec2<f32>(3.0, -1.0),
      vec2<f32>(-1.0, 3.0));
  var texcoords = array<vec2<f32>, 3>(
      vec2<f32>(0.0, 1.0),
      vec2<f32>(2.0, 1.0),
      vec2<f32>(0.0, -1.0));

  var output: VertexOutput;
  output.position = vec4<f32>(positions[index], 0.0, 1.0);
  output.uv = texcoords[index];
  return output;
}
)",
    "vs_main",
    {},
};

const ShaderSource kFS_Bicubic = {
    R"(
struct Params {
  a: vec4<f32>,
  b: vec4<f32>,
  c: vec4<f32>,
  d: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_params: Params;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

fn load_texel(p: vec2<i32>) -> vec4<f32> {
  let size = vec2<i32>(textureDimensions(u_texture));
  return textureLoad(
      u_texture, clamp(p, vec2<i32>(0), max(size - vec2<i32>(1), vec2<i32>(0))),
      0);
}

fn cubic_weight(value: f32) -> f32 {
  let x = abs(value);
  if (x < 1.0) {
    return (1.5 * x - 2.5) * x * x + 1.0;
  }
  if (x < 2.0) {
    return ((-0.5 * x + 2.5) * x - 4.0) * x + 2.0;
  }
  return 0.0;
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let src_size = u_params.a.xy;
  let dst_size = u_params.a.zw;
  let source = (floor(input.position.xy) + vec2<f32>(0.5)) * src_size / dst_size -
               vec2<f32>(0.5);
  let base = floor(source);
  let fraction = source - base;
  let origin = vec2<i32>(base);

  var accum = vec4<f32>(0.0);
  var total = 0.0;
  for (var y: i32 = -1; y <= 2; y = y + 1) {
    for (var x: i32 = -1; x <= 2; x = x + 1) {
      let wx = cubic_weight(fraction.x - f32(x));
      let wy = cubic_weight(fraction.y - f32(y));
      let weight = wx * wy;
      accum += load_texel(origin + vec2<i32>(x, y)) * weight;
      total += weight;
    }
  }
  return clamp(accum / total, vec4<f32>(0.0), vec4<f32>(1.0));
}
)",
    "fs_main",
    {
        {0,
         {{0, wgpu::ShaderStage::Fragment,
           util::BufferLayout::Uniform(false, sizeof(Upscaler::Params))}}},
        {1,
         {{0, wgpu::ShaderStage::Fragment,
           util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
          {1, wgpu::ShaderStage::Fragment,
           util::SamplerLayout::Filtering()}}},
    },
};

const ShaderSource kFS_Lanczos = {
    R"(
const PI: f32 = 3.141592653589793;

struct Params {
  a: vec4<f32>,
  b: vec4<f32>,
  c: vec4<f32>,
  d: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_params: Params;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

fn load_texel(p: vec2<i32>) -> vec4<f32> {
  let size = vec2<i32>(textureDimensions(u_texture));
  return textureLoad(
      u_texture, clamp(p, vec2<i32>(0), max(size - vec2<i32>(1), vec2<i32>(0))),
      0);
}

fn sinc(value: f32) -> f32 {
  if (abs(value) < 1.0e-6) {
    return 1.0;
  }
  let px = PI * value;
  return sin(px) / px;
}

fn lanczos_weight(value: f32) -> f32 {
  let x = abs(value);
  if (x >= 2.0) {
    return 0.0;
  }
  return sinc(x) * sinc(x * 0.5);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let src_size = u_params.a.xy;
  let dst_size = u_params.a.zw;
  let source = (floor(input.position.xy) + vec2<f32>(0.5)) * src_size / dst_size -
               vec2<f32>(0.5);
  let base = floor(source);
  let fraction = source - base;
  let origin = vec2<i32>(base);

  var accum = vec4<f32>(0.0);
  var total = 0.0;
  for (var y: i32 = -1; y <= 2; y = y + 1) {
    for (var x: i32 = -1; x <= 2; x = x + 1) {
      let wx = lanczos_weight(fraction.x - f32(x));
      let wy = lanczos_weight(fraction.y - f32(y));
      let weight = wx * wy;
      accum += load_texel(origin + vec2<i32>(x, y)) * weight;
      total += weight;
    }
  }
  return clamp(accum / total, vec4<f32>(0.0), vec4<f32>(1.0));
}
)",
    "fs_main",
    {
        {0,
         {{0, wgpu::ShaderStage::Fragment,
           util::BufferLayout::Uniform(false, sizeof(Upscaler::Params))}}},
        {1,
         {{0, wgpu::ShaderStage::Fragment,
           util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
          {1, wgpu::ShaderStage::Fragment,
           util::SamplerLayout::Filtering()}}},
    },
};

// EASU/RCAS are ports of AMD FidelityFX FSR 1's spatial upscaling and sharpening
// stages (MIT licensed).  The 32-bit EASU gather taps are expressed as twelve
// textureLoad calls so the WebGPU gather ordering cannot change the taps.
const ShaderSource kFS_FsrEasu = {
    R"(
struct Params {
  a: vec4<f32>,
  b: vec4<f32>,
  c: vec4<f32>,
  d: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_params: Params;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

struct EasuDirLen {
  dir: vec2<f32>,
  len: f32,
};

fn load_texel(p: vec2<i32>) -> vec4<f32> {
  let size = vec2<i32>(textureDimensions(u_texture));
  return textureLoad(
      u_texture, clamp(p, vec2<i32>(0), max(size - vec2<i32>(1), vec2<i32>(0))),
      0);
}

fn luma2(color: vec3<f32>) -> f32 {
  return color.b * 0.5 + (color.r * 0.5 + color.g);
}

fn easu_set(pp: vec2<f32>, s: bool, t: bool, u: bool, v: bool,
            la: f32, lb: f32, lc: f32, ld: f32, le: f32) -> EasuDirLen {
  var weight = 0.0;
  if (s) {
    weight = (1.0 - pp.x) * (1.0 - pp.y);
  }
  if (t) {
    weight = pp.x * (1.0 - pp.y);
  }
  if (u) {
    weight = (1.0 - pp.x) * pp.y;
  }
  if (v) {
    weight = pp.x * pp.y;
  }

  let dc = ld - lc;
  let cb = lc - lb;
  var len_x = max(abs(dc), abs(cb));
  len_x = select(0.0, 1.0 / max(len_x, 1.0e-10), len_x > 0.0);
  let dir_x = ld - lb;
  var lx = clamp(abs(dir_x) * len_x, 0.0, 1.0);
  lx *= lx;

  let ec = le - lc;
  let ca = lc - la;
  var len_y = max(abs(ec), abs(ca));
  len_y = select(0.0, 1.0 / max(len_y, 1.0e-10), len_y > 0.0);
  let dir_y = le - la;
  var ly = clamp(abs(dir_y) * len_y, 0.0, 1.0);
  ly *= ly;

  return EasuDirLen(vec2<f32>(dir_x * weight, dir_y * weight),
                    (lx + ly) * weight);
}

fn easu_tap(off: vec2<f32>, dir: vec2<f32>, len2: vec2<f32>,
            lob: f32, clp: f32, color: vec3<f32>) -> vec4<f32> {
  let v = vec2<f32>(off.x * dir.x + off.y * dir.y,
                    off.x * (-dir.y) + off.y * dir.x) * len2;
  let d2 = min(dot(v, v), clp);
  var wb = (2.0 / 5.0) * d2 - 1.0;
  var wa = lob * d2 - 1.0;
  wb *= wb;
  wa *= wa;
  wb = (25.0 / 16.0) * wb - (25.0 / 16.0 - 1.0);
  let w = wb * wa;
  return vec4<f32>(color * w, w);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let ip = floor(input.position.xy);
  let source_pos = ip * u_params.a.xy + u_params.a.zw;
  let fp = floor(source_pos);
  let fraction = source_pos - fp;
  let origin = vec2<i32>(fp);

  let b = load_texel(origin + vec2<i32>(0, -1));
  let c = load_texel(origin + vec2<i32>(1, -1));
  let e = load_texel(origin + vec2<i32>(-1, 0));
  let f = load_texel(origin + vec2<i32>(0, 0));
  let g = load_texel(origin + vec2<i32>(1, 0));
  let h = load_texel(origin + vec2<i32>(2, 0));
  let i = load_texel(origin + vec2<i32>(-1, 1));
  let j = load_texel(origin + vec2<i32>(0, 1));
  let k = load_texel(origin + vec2<i32>(1, 1));
  let l = load_texel(origin + vec2<i32>(2, 1));
  let n = load_texel(origin + vec2<i32>(0, 2));
  let o = load_texel(origin + vec2<i32>(1, 2));

  let lb = luma2(b.rgb);
  let lc = luma2(c.rgb);
  let le = luma2(e.rgb);
  let lf = luma2(f.rgb);
  let lg = luma2(g.rgb);
  let lh = luma2(h.rgb);
  let li = luma2(i.rgb);
  let lj = luma2(j.rgb);
  let lk = luma2(k.rgb);
  let ll = luma2(l.rgb);
  let ln = luma2(n.rgb);
  let lo = luma2(o.rgb);

  var dir = vec2<f32>(0.0);
  var len = 0.0;
  let s0 = easu_set(fraction, true, false, false, false, lb, le, lf, lg, lj);
  let s1 = easu_set(fraction, false, true, false, false, lc, lf, lg, lh, lk);
  let s2 = easu_set(fraction, false, false, true, false, lf, li, lj, lk, ln);
  let s3 = easu_set(fraction, false, false, false, true, lg, lj, lk, ll, lo);
  dir += s0.dir;
  dir += s1.dir;
  dir += s2.dir;
  dir += s3.dir;
  len += s0.len + s1.len + s2.len + s3.len;

  let dir_len2 = dot(dir, dir);
  let zero = dir_len2 < (1.0 / 32768.0);
  var dir_norm = dir * inverseSqrt(max(dir_len2, 1.0e-10));
  dir_norm = select(dir_norm, vec2<f32>(1.0, 0.0), zero);

  len *= 0.5;
  len *= len;

  let stretch = dot(dir_norm, dir_norm) /
                max(abs(dir_norm.x), abs(dir_norm.y));
  let len2 = vec2<f32>(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
  let lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
  let clp = 1.0 / lob;

  let min4 = min(min(f.rgb, g.rgb), min(j.rgb, k.rgb));
  let max4 = max(max(f.rgb, g.rgb), max(j.rgb, k.rgb));

  var accum = vec3<f32>(0.0);
  var total = 0.0;

  let tb = easu_tap(vec2<f32>(0.0, -1.0) - fraction, dir_norm, len2, lob, clp,
                    b.rgb);
  accum += tb.rgb;
  total += tb.w;
  let tc = easu_tap(vec2<f32>(1.0, -1.0) - fraction, dir_norm, len2, lob, clp,
                    c.rgb);
  accum += tc.rgb;
  total += tc.w;
  let ti = easu_tap(vec2<f32>(-1.0, 1.0) - fraction, dir_norm, len2, lob, clp,
                    i.rgb);
  accum += ti.rgb;
  total += ti.w;
  let tj = easu_tap(vec2<f32>(0.0, 1.0) - fraction, dir_norm, len2, lob, clp,
                    j.rgb);
  accum += tj.rgb;
  total += tj.w;
  let tf = easu_tap(vec2<f32>(0.0, 0.0) - fraction, dir_norm, len2, lob, clp,
                    f.rgb);
  accum += tf.rgb;
  total += tf.w;
  let te = easu_tap(vec2<f32>(-1.0, 0.0) - fraction, dir_norm, len2, lob, clp,
                    e.rgb);
  accum += te.rgb;
  total += te.w;
  let tk = easu_tap(vec2<f32>(1.0, 1.0) - fraction, dir_norm, len2, lob, clp,
                    k.rgb);
  accum += tk.rgb;
  total += tk.w;
  let tl = easu_tap(vec2<f32>(2.0, 1.0) - fraction, dir_norm, len2, lob, clp,
                    l.rgb);
  accum += tl.rgb;
  total += tl.w;
  let th = easu_tap(vec2<f32>(2.0, 0.0) - fraction, dir_norm, len2, lob, clp,
                    h.rgb);
  accum += th.rgb;
  total += th.w;
  let tg = easu_tap(vec2<f32>(1.0, 0.0) - fraction, dir_norm, len2, lob, clp,
                    g.rgb);
  accum += tg.rgb;
  total += tg.w;
  let to = easu_tap(vec2<f32>(1.0, 2.0) - fraction, dir_norm, len2, lob, clp,
                    o.rgb);
  accum += to.rgb;
  total += to.w;
  let tn = easu_tap(vec2<f32>(0.0, 2.0) - fraction, dir_norm, len2, lob, clp,
                    n.rgb);
  accum += tn.rgb;
  total += tn.w;

  let inv_total = select(0.0, 1.0 / total, total > 0.0);
  let result = clamp(accum * inv_total, min4, max4);
  return vec4<f32>(result, f.a);
}
)",
    "fs_main",
    {
        {0,
         {{0, wgpu::ShaderStage::Fragment,
           util::BufferLayout::Uniform(false, sizeof(Upscaler::Params))}}},
        {1,
         {{0, wgpu::ShaderStage::Fragment,
           util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
          {1, wgpu::ShaderStage::Fragment,
           util::SamplerLayout::Filtering()}}},
    },
};

const ShaderSource kFS_FsrRcas = {
    R"(
struct Params {
  a: vec4<f32>,
  b: vec4<f32>,
  c: vec4<f32>,
  d: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_params: Params;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

struct FragmentInput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

fn load_texel(p: vec2<i32>) -> vec4<f32> {
  let size = vec2<i32>(textureDimensions(u_texture));
  return textureLoad(
      u_texture, clamp(p, vec2<i32>(0), max(size - vec2<i32>(1), vec2<i32>(0))),
      0);
}

fn safe_rcp(value: f32) -> f32 {
  return select(0.0, 1.0 / value, abs(value) > 1.0e-8);
}

@fragment
fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
  let ip = vec2<i32>(floor(input.position.xy));
  let b = load_texel(ip + vec2<i32>(0, -1));
  let d = load_texel(ip + vec2<i32>(-1, 0));
  let e = load_texel(ip);
  let f = load_texel(ip + vec2<i32>(1, 0));
  let h = load_texel(ip + vec2<i32>(0, 1));

  let mn_r = min(min(b.r, d.r), min(f.r, h.r));
  let mn_g = min(min(b.g, d.g), min(f.g, h.g));
  let mn_b = min(min(b.b, d.b), min(f.b, h.b));
  let mx_r = max(max(b.r, d.r), max(f.r, h.r));
  let mx_g = max(max(b.g, d.g), max(f.g, h.g));
  let mx_b = max(max(b.b, d.b), max(f.b, h.b));

  let hit_min_r = min(mn_r, e.r) * safe_rcp(4.0 * mx_r);
  let hit_min_g = min(mn_g, e.g) * safe_rcp(4.0 * mx_g);
  let hit_min_b = min(mn_b, e.b) * safe_rcp(4.0 * mx_b);
  let hit_max_r = (1.0 - max(mx_r, e.r)) * safe_rcp(4.0 * mn_r - 4.0);
  let hit_max_g = (1.0 - max(mx_g, e.g)) * safe_rcp(4.0 * mn_g - 4.0);
  let hit_max_b = (1.0 - max(mx_b, e.b)) * safe_rcp(4.0 * mn_b - 4.0);

  let lobe_r = max(-hit_min_r, hit_max_r);
  let lobe_g = max(-hit_min_g, hit_max_g);
  let lobe_b = max(-hit_min_b, hit_max_b);
  let lobe = clamp(max(lobe_r, max(lobe_g, lobe_b)), -0.1875, 0.0) *
             u_params.a.x;
  let rcp_lobe = 1.0 / (4.0 * lobe + 1.0);

  let r = (lobe * b.r + lobe * d.r + lobe * h.r + lobe * f.r + e.r) * rcp_lobe;
  let g = (lobe * b.g + lobe * d.g + lobe * h.g + lobe * f.g + e.g) * rcp_lobe;
  let bl =
      (lobe * b.b + lobe * d.b + lobe * h.b + lobe * f.b + e.b) * rcp_lobe;
  return vec4<f32>(r, g, bl, e.a);
}
)",
    "fs_main",
    {
        {0,
         {{0, wgpu::ShaderStage::Fragment,
           util::BufferLayout::Uniform(false, sizeof(Upscaler::Params))}}},
        {1,
         {{0, wgpu::ShaderStage::Fragment,
           util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
          {1, wgpu::ShaderStage::Fragment,
           util::SamplerLayout::Filtering()}}},
    },
};

} // namespace

Upscaler::Mode Upscaler::ParseMode(std::string_view name) {
  std::string lower;
  lower.reserve(name.size());
  for (char c : name)
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

  if (lower.empty() || lower == "off" || lower == "none")
    return Mode::kOff;
  if (lower == "nearest")
    return Mode::kNearest;
  if (lower == "linear" || lower == "bilinear")
    return Mode::kLinear;
  if (lower == "bicubic")
    return Mode::kBicubic;
  if (lower == "lanczos")
    return Mode::kLanczos;
  if (lower == "fsr1" || lower == "fsr")
    return Mode::kFsr1;
  return Mode::kOff;
}

const char* Upscaler::ModeName(Mode mode) {
  switch (mode) {
    case Mode::kOff:
      return "off";
    case Mode::kNearest:
      return "nearest";
    case Mode::kLinear:
      return "linear";
    case Mode::kBicubic:
      return "bicubic";
    case Mode::kLanczos:
      return "lanczos";
    case Mode::kFsr1:
      return "fsr1";
  }
  return "off";
}

Upscaler::Upscaler(Mode mode) : mode_(mode) {
  if (!UsesShader())
    return;

  CreatePipelines();

  wgpu::SamplerDescriptor sampler_desc;
  sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
  sampler_desc.magFilter = wgpu::FilterMode::Linear;
  sampler_desc.minFilter = wgpu::FilterMode::Linear;
  sampler_ = g_device.CreateSampler(&sampler_desc);

  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  buffer_desc.size = sizeof(Params);
  params_[0] = g_device.CreateBuffer(&buffer_desc);
  params_[1] = g_device.CreateBuffer(&buffer_desc);
}

Upscaler::~Upscaler() = default;

bool Upscaler::UsesShader() const {
  return mode_ == Mode::kBicubic || mode_ == Mode::kLanczos ||
         mode_ == Mode::kFsr1;
}

void Upscaler::CreatePipelines() {
  wgpu::PrimitiveState primitive;
  primitive.topology = wgpu::PrimitiveTopology::TriangleList;

  const auto make_state = [&](const ShaderSource& fragment) {
    Pipeline pipeline(kVS_Fullscreen, fragment, {});
    return pipeline.MakeState(
        primitive, std::nullopt,
        {wgpu::ColorTargetState{.format = wgpu::TextureFormat::RGBA8Unorm}});
  };

  switch (mode_) {
    case Mode::kBicubic:
      bicubic_ = make_state(kFS_Bicubic);
      break;
    case Mode::kLanczos:
      lanczos_ = make_state(kFS_Lanczos);
      break;
    case Mode::kFsr1:
      fsr_easu_ = make_state(kFS_FsrEasu);
      fsr_rcas_ = make_state(kFS_FsrRcas);
      break;
    default:
      break;
  }
}

void Upscaler::EnsureOutputs(const glm::ivec2& size, int count) {
  count = std::clamp(count, 0, 2);
  if (output_count_ == count && output_size_.x == size.x &&
      output_size_.y == size.y)
    return;

  output_count_ = count;
  output_size_ = size;
  for (int i = 0; i < count; ++i) {
    wgpu::TextureDescriptor texture_desc;
    texture_desc.usage = wgpu::TextureUsage::RenderAttachment |
                         wgpu::TextureUsage::TextureBinding;
    texture_desc.dimension = wgpu::TextureDimension::e2D;
    texture_desc.size.width = static_cast<std::uint32_t>(size.x);
    texture_desc.size.height = static_cast<std::uint32_t>(size.y);
    texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    outputs_[i] = g_device.CreateTexture(&texture_desc);
    output_views_[i] = outputs_[i].CreateView(nullptr);
  }
  ++generation_;
}

void Upscaler::WriteParams(int index, const Params& params) {
  g_queue.WriteBuffer(params_[index], 0, &params, sizeof(params));
}

wgpu::BindGroup Upscaler::MakeSourceGroup(
    const wgpu::RenderPipeline& state, wgpu::TextureView view) {
  return util::CreateBindGroup(
      state.GetBindGroupLayout(1),
      {{0, util::TextureViewSet(view)}, {1, util::SamplerSet(sampler_)}});
}

wgpu::BindGroup Upscaler::MakeParamsGroup(const wgpu::RenderPipeline& state,
                                          wgpu::Buffer buffer) {
  return util::CreateBindGroup(state.GetBindGroupLayout(0),
                               {{0, util::BufferSet(buffer)}});
}

void Upscaler::Draw(wgpu::CommandEncoder encoder,
                    const wgpu::RenderPipeline& state,
                    wgpu::TextureView target,
                    wgpu::BindGroup params,
                    wgpu::BindGroup source) {
  wgpu::RenderPassColorAttachment color_attachment = {
      .view = target,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0},
  };
  wgpu::RenderPassDescriptor pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };

  auto pass = encoder.BeginRenderPass(&pass_desc);
  pass.SetPipeline(state);
  pass.SetBindGroup(0, params, 0, nullptr);
  pass.SetBindGroup(1, source, 0, nullptr);
  pass.Draw(3, 1, 0, 0);
  pass.End();
}

wgpu::TextureView Upscaler::Run(wgpu::CommandEncoder encoder,
                                wgpu::TextureView source,
                                const glm::ivec2& source_size,
                                const glm::ivec2& output_size) {
  if (!UsesShader() || output_size.x <= 0 || output_size.y <= 0)
    return {};

  const float src_width = static_cast<float>(source_size.x);
  const float src_height = static_cast<float>(source_size.y);
  const float dst_width = static_cast<float>(output_size.x);
  const float dst_height = static_cast<float>(output_size.y);

  if (mode_ == Mode::kFsr1) {
    EnsureOutputs(output_size, 2);

    Params easu = {};
    easu.a = glm::vec4(src_width / dst_width, src_height / dst_height,
                       0.5f * src_width / dst_width - 0.5f,
                       0.5f * src_height / dst_height - 0.5f);
    WriteParams(0, easu);
    Draw(encoder, fsr_easu_, output_views_[0],
         MakeParamsGroup(fsr_easu_, params_[0]),
         MakeSourceGroup(fsr_easu_, source));

    Params rcas = {};
    rcas.a = glm::vec4(std::exp2(-0.2f), 0.0f, 0.0f, 0.0f);
    WriteParams(1, rcas);
    Draw(encoder, fsr_rcas_, output_views_[1],
         MakeParamsGroup(fsr_rcas_, params_[1]),
         MakeSourceGroup(fsr_rcas_, output_views_[0]));
    return output_views_[1];
  }

  EnsureOutputs(output_size, 1);
  Params params = {};
  params.a = glm::vec4(src_width, src_height, dst_width, dst_height);
  WriteParams(0, params);

  const wgpu::RenderPipeline& state =
      mode_ == Mode::kBicubic ? bicubic_ : lanczos_;
  Draw(encoder, state, output_views_[0],
       MakeParamsGroup(state, params_[0]),
       MakeSourceGroup(state, source));
  return output_views_[0];
}

}
