// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_BUILTIN_WGSL_H_
#define RENDERER_PIPELINE_BUILTIN_WGSL_H_

#include <string>

namespace renderer {

///
// type:
//   basis shader
///
// entry:
//   vertexMain fragmentMain
///
// vertex:
//   @<0>: vec4<f32>
//   @<1>: vec2<f32>
//   @<2>: vec4<f32>
///
// bind:
//   @<0>: mat4x4
//   @<1>: texture2d<f32>
//   @<2>: sampler
///
extern const std::string kBaseRenderWGSL;

///
// type:
//   color shader
///
// entry:
//   vertexMain fragmentMain
///
// vertex:
//   @<0>: vec4<f32>
//   @<1>: vec2<f32>
//   @<2>: vec4<f32>
///
// bind:
//   @<0>: mat4x4
///
extern const std::string kColorRenderWGSL;

}  // namespace renderer

#endif  //! RENDERER_PIPELINE_BUILTIN_WGSL_H_