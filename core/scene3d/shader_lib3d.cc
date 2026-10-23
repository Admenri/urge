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

#include "core/scene3d/shader_lib3d.h"

namespace urge {

namespace {

constexpr std::string_view kPrelude = R"(
struct UrgeLight {
  position_type: vec4<f32>,
  direction_range: vec4<f32>,
  color_intensity: vec4<f32>,
};

struct UrgeView {
  view_proj: mat4x4<f32>,
  view: mat4x4<f32>,
  camera_position: vec4<f32>,
  ambient: vec4<f32>,
  light_view_proj: mat4x4<f32>,
  lights: array<UrgeLight, 8>,
  cascade_splits: vec4<f32>,
  cascade_params: vec4<f32>,
  cascade_matrices: array<mat4x4<f32>, 4>,
  camera_forward: vec4<f32>,
  point_shadow_params: vec4<f32>,
  point_face_view_proj: array<mat4x4<f32>, 6>,
};

struct UrgeMesh {
  model: mat4x4<f32>,
  normal: mat4x4<f32>,
};

struct UrgeGlobals {
  time: vec4<f32>,
  viewport: vec4<f32>,
};

struct UrgeVSOut {
  @builtin(position) position: vec4<f32>,
  @location(0) world: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) uv: vec2<f32>,
};

@group(0) @binding(0) var<uniform> u_view: UrgeView;
@group(0) @binding(1) var u_shadow_map: texture_depth_2d;
@group(0) @binding(2) var u_shadow_sampler: sampler_comparison;
@group(0) @binding(3) var u_shadow_cascades: texture_depth_2d_array;
@group(0) @binding(4) var u_cascade_sampler: sampler_comparison;
@group(0) @binding(5) var u_shadow_cube: texture_depth_cube;
@group(0) @binding(6) var u_cube_sampler: sampler_comparison;

@group(1) @binding(0) var<uniform> u_mesh: UrgeMesh;
@group(1) @binding(1) var<storage, read> u_joints: array<mat4x4<f32>>;

@group(3) @binding(0) var<uniform> u_globals: UrgeGlobals;

fn urge_model() -> mat4x4<f32> {
  return u_mesh.model;
}

fn urge_world(local_position: vec4<f32>) -> vec4<f32> {
  return u_mesh.model * local_position;
}

fn urge_clip(local_position: vec4<f32>) -> vec4<f32> {
  return u_view.view_proj * u_mesh.model * local_position;
}

fn urge_world_normal(local_normal: vec3<f32>) -> vec3<f32> {
  return normalize((u_mesh.normal * vec4<f32>(local_normal, 0.0)).xyz);
}

fn urge_skinned_position(local_position: vec4<f32>,
                         joints: vec4<u32>,
                         weights: vec4<f32>) -> vec4<f32> {
  var result = vec4<f32>(0.0);
  for (var i = 0u; i < 4u; i = i + 1u) {
    let weight = weights[i];
    if (weight <= 0.0) {
      continue;
    }
    result = result + (u_joints[joints[i]] * local_position) * weight;
  }
  return result;
}

fn urge_skinned_normal(local_normal: vec3<f32>,
                       joints: vec4<u32>,
                       weights: vec4<f32>) -> vec3<f32> {
  var result = vec3<f32>(0.0);
  for (var i = 0u; i < 4u; i = i + 1u) {
    let weight = weights[i];
    if (weight <= 0.0) {
      continue;
    }
    result = result +
             (u_joints[joints[i]] * vec4<f32>(local_normal, 0.0)).xyz * weight;
  }
  return normalize(result);
}

fn urge_camera_position() -> vec3<f32> {
  return u_view.camera_position.xyz;
}

fn urge_view_depth(world_position: vec3<f32>) -> f32 {
  return -(u_view.view * vec4<f32>(world_position, 1.0)).z;
}

fn urge_light_count() -> u32 {
  return u32(u_view.ambient.w);
}

fn urge_cascade_count() -> u32 {
  return u32(u_view.cascade_params.x);
}

fn urge_shadow_texel() -> f32 {
  return u_view.cascade_params.y;
}

fn urge_cascade_index(world_position: vec3<f32>) -> i32 {
  let count = urge_cascade_count();
  if (count <= 1u) {
    return 0;
  }
  let depth = urge_view_depth(world_position);
  var index = i32(count) - 1;
  for (var i = 0u; i < count; i = i + 1u) {
    if (depth < u_view.cascade_splits[i]) {
      index = i32(i);
      break;
    }
  }
  return index;
}

fn urge_pcf_shadow_2d(uv: vec2<f32>, depth_ref: f32) -> f32 {
  let texel = urge_shadow_texel();
  var sum = 0.0;
  for (var y = -1; y <= 1; y = y + 1) {
    for (var x = -1; x <= 1; x = x + 1) {
      let offset = vec2<f32>(f32(x), f32(y)) * texel;
      sum = sum +
            textureSampleCompareLevel(u_shadow_map, u_shadow_sampler,
                                      uv + offset, depth_ref);
    }
  }
  return sum / 9.0;
}

fn urge_pcf_shadow_array(uv: vec2<f32>, layer: i32, depth_ref: f32) -> f32 {
  let texel = urge_shadow_texel();
  var sum = 0.0;
  for (var y = -1; y <= 1; y = y + 1) {
    for (var x = -1; x <= 1; x = x + 1) {
      let offset = vec2<f32>(f32(x), f32(y)) * texel;
      sum = sum +
            textureSampleCompareLevel(u_shadow_cascades, u_cascade_sampler,
                                      uv + offset, layer, depth_ref);
    }
  }
  return sum / 9.0;
}

fn urge_shadow_factor(world_position: vec3<f32>, normal: vec3<f32>) -> f32 {
  let count = urge_cascade_count();
  if (count == 0u) {
    return 1.0;
  }

  let cascade = urge_cascade_index(world_position);
  let split = max(u_view.cascade_splits[cascade], 1.0);
  let texel = 2.0 * split * u_view.cascade_params.y;
  let offset_position = world_position + normal * texel * 2.0;

  let clip = u_view.cascade_matrices[cascade] * vec4<f32>(offset_position, 1.0);
  let ndc = clip.xyz / clip.w;
  let uv = vec2<f32>(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
  if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0) {
    return 1.0;
  }
  return urge_pcf_shadow_array(uv, cascade, ndc.z - 0.0004);
}

fn urge_point_shadow_factor(world_position: vec3<f32>, normal: vec3<f32>) -> f32 {
  let light_index = i32(u_view.point_shadow_params.x);
  if (light_index < 0) {
    return 1.0;
  }

  let light_position = u_view.lights[light_index].position_type.xyz;
  let to_fragment = world_position - light_position;
  let distance = length(to_fragment);
  let range = max(u_view.point_shadow_params.y, 0.01);
  if (distance >= range) {
    return 1.0;
  }

  let direction = to_fragment / max(distance, 0.0001);
  let axis = abs(direction);

  var face = 0;
  if (axis.x >= axis.y && axis.x >= axis.z) {
    face = select(1, 0, direction.x > 0.0);
  } else if (axis.y >= axis.z) {
    face = select(3, 2, direction.y > 0.0);
  } else {
    face = select(5, 4, direction.z > 0.0);
  }

  let clip = u_view.point_face_view_proj[face] * vec4<f32>(world_position, 1.0);
  let ndc = clip.xyz / clip.w;

  let facing = clamp(dot(normal, -direction), 0.0, 1.0);
  let bias = max(0.0008, 0.004 * (1.0 - facing));
  let depth_ref = ndc.z - bias;

  var sum = 0.0;
  let texel = urge_shadow_texel();
  for (var y = -1; y <= 1; y = y + 1) {
    for (var x = -1; x <= 1; x = x + 1) {
      let offset = vec2<f32>(f32(x), f32(y)) * texel;
      sum = sum +
            textureSampleCompareLevel(u_shadow_cube, u_cube_sampler,
                                      direction + vec3<f32>(offset, 0.0),
                                      depth_ref);
    }
  }
  return sum / 9.0;
}

fn urge_distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
  let a = roughness * roughness;
  let a2 = a * a;
  let d = n_dot_h * n_dot_h * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159265 * d * d, 0.0001);
}

fn urge_geometry_smith(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
  let k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
  let gv = n_dot_v / (n_dot_v * (1.0 - k) + k);
  let gl = n_dot_l / (n_dot_l * (1.0 - k) + k);
  return gv * gl;
}

fn urge_fresnel_schlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
  return f0 + (vec3<f32>(1.0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn urge_brdf(normal: vec3<f32>,
             view_dir: vec3<f32>,
             light_dir: vec3<f32>,
             albedo: vec3<f32>,
             roughness: f32,
             metallic: f32) -> vec3<f32> {
  let n_dot_l = max(dot(normal, light_dir), 0.0);
  if (n_dot_l <= 0.0) {
    return vec3<f32>(0.0);
  }

  let n_dot_v = max(dot(normal, view_dir), 0.0001);
  let half_dir = normalize(light_dir + view_dir);
  let n_dot_h = max(dot(normal, half_dir), 0.0);
  let h_dot_v = max(dot(half_dir, view_dir), 0.0);

  let f0 = mix(vec3<f32>(0.04), albedo, metallic);
  let d = urge_distribution_ggx(n_dot_h, max(roughness, 0.04));
  let g = urge_geometry_smith(n_dot_v, n_dot_l, max(roughness, 0.04));
  let f = urge_fresnel_schlick(h_dot_v, f0);

  let specular = (d * g) * f / max(4.0 * n_dot_v * n_dot_l, 0.0001);
  let diffuse = (vec3<f32>(1.0) - f) * (1.0 - metallic) * albedo / 3.14159265;
  return (diffuse + specular) * n_dot_l;
}

fn urge_shade(world_position: vec3<f32>,
              normal: vec3<f32>,
              albedo: vec3<f32>,
              roughness: f32,
              metallic: f32) -> vec3<f32> {
  let view_dir = normalize(u_view.camera_position.xyz - world_position);
  var color = albedo * u_view.ambient.rgb;

  let count = urge_light_count();
  for (var i = 0u; i < count; i = i + 1u) {
    let light = u_view.lights[i];
    let light_type = light.position_type.w;
    let radiance = light.color_intensity.rgb * light.color_intensity.w;

    var light_dir = normalize(-light.direction_range.xyz);
    var attenuation = 1.0;

    if (light_type > 0.5) {
      let to_light = light.position_type.xyz - world_position;
      let distance = length(to_light);
      light_dir = to_light / max(distance, 0.0001);

      let range = max(light.direction_range.w, 0.001);
      let ratio = clamp(distance / range, 0.0, 1.0);
      attenuation = (1.0 - ratio * ratio) * (1.0 - ratio * ratio);

      if (light_type > 1.5) {
        let spot = dot(-light_dir, normalize(light.direction_range.xyz));
        attenuation = attenuation * clamp(spot, 0.0, 1.0);
      }
    }

    var shadow = 1.0;
    if (light_type < 0.5) {
      shadow = urge_shadow_factor(world_position, normal);
    } else if (i32(u_view.point_shadow_params.x) == i32(i)) {
      shadow = urge_point_shadow_factor(world_position, normal);
    }

    color = color + urge_brdf(normal, view_dir, light_dir, albedo, roughness,
                              metallic) *
                    radiance * attenuation * shadow;
  }

  return color;
}

fn urge_aces_tonemap(color: vec3<f32>) -> vec3<f32> {
  let a = 2.51;
  let b = 0.03;
  let c = 2.43;
  let d = 0.59;
  let e = 0.14;
  return clamp((color * (a * color + b)) / (color * (c * color + d) + e),
               vec3<f32>(0.0), vec3<f32>(1.0));
}

fn urge_linear_to_srgb(color: vec3<f32>) -> vec3<f32> {
  let low = color * 12.92;
  let high = 1.055 * pow(max(color, vec3<f32>(0.0)), vec3<f32>(1.0 / 2.4)) - 0.055;
  return select(high, low, color <= vec3<f32>(0.0031308));
}

fn urge_fullscreen_position(index: u32) -> vec4<f32> {
  let x = f32((index << 1u) & 2u) * 2.0 - 1.0;
  let y = f32(index & 2u) * 2.0 - 1.0;
  return vec4<f32>(x, y, 0.0, 1.0);
}

fn urge_fullscreen_uv(position: vec4<f32>) -> vec2<f32> {
  return vec2<f32>(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5);
}

fn urge_screen_uv(fragment_position: vec4<f32>) -> vec2<f32> {
  return fragment_position.xy * u_globals.viewport.zw;
}
)";

}

std::string_view ShaderPrelude3D() {
  return kPrelude;
}

std::string ComposeShader3D(std::string_view user_source) {
  if (user_source.find("struct UrgeView") != std::string_view::npos)
    return std::string(user_source);

  std::string result;
  result.reserve(kPrelude.size() + user_source.size() + 2);
  result.append(kPrelude);
  result.append("\n");
  result.append(user_source);
  return result;
}

}
