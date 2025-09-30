// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_BUILTIN_HLSL_H_
#define RENDERER_PIPELINE_BUILTIN_HLSL_H_

#include <string>

namespace renderer {

///
// type:
//   basis shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
///
extern const std::string kHLSL_BaseRender_Vertex;
extern const std::string kHLSL_BaseRender_Pixel;

///
// type:
//   bitmapblt shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
//   { Texture2D }
///
extern const std::string kHLSL_BitmapBltRender_Vertex;
extern const std::string kHLSL_BitmapBltRender_Pixel;

///
// type:
//   bitmapclipblt shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
//   { Texture2D }
//   { Texture2D }
///
extern const std::string kHLSL_BitmapClipBltRender_Vertex;
extern const std::string kHLSL_BitmapClipBltRender_Pixel;

///
// type:
//   color shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
///
extern const std::string kHLSL_ColorRender_Vertex;
extern const std::string kHLSL_ColorRender_Pixel;

///
// type:
//   flat shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
//   { float4, float4 }
///
extern const std::string kHLSL_FlatRender_Vertex;
extern const std::string kHLSL_FlatRender_Pixel;

///
// type:
//   sprite shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
//   < { float4, float4, float2, float2, float2, float, float, float, float } >
//
// macros:
//   STORAGE_BUFFER_SUPPORT
///
extern const std::string kHLSL_SpriteRender_Vertex;
extern const std::string kHLSL_SpriteRender_Pixel;

///
// type:
//   alpha transition shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// bind:
//   { Texture2D, Texture2D }
///
extern const std::string kHLSL_AlphaTransitionRender_Vertex;
extern const std::string kHLSL_AlphaTransitionRender_Pixel;

///
// type:
//   mapping transition shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// bind:
//   { Texture2D, Texture2D, Texture2D }
///
extern const std::string kHLSL_MappingTransitionRender_Vertex;
extern const std::string kHLSL_MappingTransitionRender_Pixel;

///
// type:
//   tilemap shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// bind:
//   { float4x4, float4x4 }
//   { Texture2D }
//   { float2, float2, float, float }
///
extern const std::string kHLSL_TilemapRender_Vertex;
extern const std::string kHLSL_TilemapRender_Pixel;

///
// type:
//   tilemap2 shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// bind:
//   { float4x4, float4x4 }
//   { Texture2D }
//   { float2, float2, float2, float }
///
extern const std::string kHLSL_Tilemap2Render_Vertex;
extern const std::string kHLSL_Tilemap2Render_Pixel;

///
// type:
//   bitmap hue shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { Texture2D }
///
extern const std::string kHLSL_BitmapHueRender_Vertex;
extern const std::string kHLSL_BitmapHueRender_Pixel;

///
// type:
//   yuv shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float2, float4>
///
// resource:
//   { Texture2D x 3 }
///
extern const std::string kHLSL_YUVRender_Vertex;
extern const std::string kHLSL_YUVRender_Pixel;

}  // namespace renderer

#endif  // !RENDERER_PIPELINE_BUILTIN_HLSL_H_
