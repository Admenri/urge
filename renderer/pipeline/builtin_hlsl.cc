// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/builtin_hlsl.h"

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

const base::String kHLSL_BaseRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_Transform.ProjMat, VSIn.Pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  frag.a *= PSIn.Color.a;
  PSOut.Color = frag;
}

)";

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

const base::String kHLSL_BitmapBltRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : TEXCOORD1;
  float Alpha : NORMAL0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_Transform.ProjMat, VSIn.Pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
  PSIn.Alpha = VSIn.Color.w;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

Texture2D u_DstTexture;
SamplerState u_DstTexture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 srcFrag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  float4 dstFrag = u_DstTexture.Sample(u_DstTexture_sampler, PSIn.Color.xy);

  float srcAlpha = srcFrag.a * PSIn.Alpha;
  float3 resultRGB = (srcFrag.rgb * srcAlpha) + (dstFrag.rgb * dstFrag.a * (1.0 - srcAlpha));
  float resultAlpha = srcAlpha + (dstFrag.a * (1.0 - srcAlpha));
  
  resultRGB = (resultAlpha <= 0.0 ? srcFrag.rgb : resultRGB / resultAlpha);

  PSOut.Color.rgb = resultRGB;
  PSOut.Color.a = resultAlpha;
}

)";

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

const base::String kHLSL_ColorRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_Transform.ProjMat, VSIn.Pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  PSOut.Color = PSIn.Color;
}

)";

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

const base::String kHLSL_FlatRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_Transform.ProjMat, VSIn.Pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

struct FlatParams {
  float4 Color;
  float4 Tone;
};

cbuffer FlatUniformConstants {
  FlatParams u_Effect;
};

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

static const float3 lumaF = float3(0.299, 0.587, 0.114);

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);

  // Tone
  float luma = dot(frag.rgb, lumaF);
  frag.rgb = lerp(frag.rgb, float3(luma, luma, luma), u_Effect.Tone.w);
  frag.rgb += u_Effect.Tone.rgb;

  // Opacity
  frag.a *= PSIn.Color.a;

  // Color
  frag.rgb = lerp(frag.rgb, u_Effect.Color.rgb, u_Effect.Color.a);

  PSOut.Color = frag;
}

)";

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
//   < { float4, float2 } >
//   < { float2, float2, float2, float, float4, float4, float, float, float } >
///

const base::String kHLSL_SpriteRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct SpriteParam {
  float4 Color;
  float4 Tone;
  float4 Position;
  float4 Origin;
  float4 Scale;
  float4 Rotation;
  float4 Opacity;
  float4 BushDepthAndOpacity;
};

#if STORAGE_BUFFER_SUPPORT
StructuredBuffer<SpriteParam> u_Params;
#else
cbuffer SpriteUniformConstants {
  SpriteParam u_Effect;
};
#endif // STORAGE_BUFFER_SUPPORT

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;

  uint VertexIdx : SV_VertexID;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : NORMAL0;
  float4 Tone : NORMAL1;
  float Opacity : NORMAL2;
  float BushDepth : NORMAL3;
  float BushOpacity : NORMAL4;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
#if STORAGE_BUFFER_SUPPORT
#if defined(GLSL)
  int vertex_id = int(VSIn.VertexIdx);
#else
  uint vertex_id = VSIn.VertexIdx;
#endif // GLSL
  SpriteParam effect = u_Params[vertex_id / 4];
#else
  SpriteParam effect = u_Effect;
#endif // STORAGE_BUFFER_SUPPORT

  float sine = sin(effect.Rotation.x);
  float cosine = cos(effect.Rotation.x);
  float sxs = effect.Scale.x * sine;
  float sxc = effect.Scale.x * cosine;
  float sys = effect.Scale.y * sine;
  float syc = effect.Scale.y * cosine;
  float tx = -effect.Origin.x * sxc - effect.Origin.y * sys + effect.Position.x;
  float ty = effect.Origin.x * sxs - effect.Origin.y * syc + effect.Position.y;

  float4 transPos;
  transPos.x = VSIn.Pos.x * sxc + VSIn.Pos.y * sys + tx;
  transPos.y = -VSIn.Pos.x * sxs + VSIn.Pos.y * syc + ty;
  transPos.z = VSIn.Pos.z;
  transPos.w = VSIn.Pos.w;

  PSIn.Pos = mul(u_Transform.ProjMat, transPos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = effect.Color;
  PSIn.Tone = effect.Tone;
  PSIn.Opacity = effect.Opacity.x;
  PSIn.BushDepth = effect.BushDepthAndOpacity.x;
  PSIn.BushOpacity = effect.BushDepthAndOpacity.y;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

static const float3 lumaF = float3(0.299, 0.587, 0.114);

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  
  // Tone
  float luma = dot(frag.rgb, lumaF);
  frag.rgb = lerp(frag.rgb, float3(luma, luma, luma), PSIn.Tone.w);
  frag.rgb += PSIn.Tone.rgb;
    
  // Color
  frag.a *= PSIn.Opacity;
  frag.rgb = lerp(frag.rgb, PSIn.Color.rgb, PSIn.Color.a);
    
  // Bush
  float currentPos = PSIn.UV.y;
  float underBush = (currentPos > PSIn.BushDepth) ? 0.0 : 1.0;
  frag.a *= clamp(PSIn.BushOpacity + underBush, 0.0, 1.0);

  PSOut.Color = frag;
}

)";

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

const base::String kHLSL_AlphaTransitionRender = R"(

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = VSIn.Pos;
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

Texture2D u_FrozenTexture;
SamplerState u_FrozenTexture_sampler;

Texture2D u_CurrentTexture;
SamplerState u_CurrentTexture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frozenFrag = u_FrozenTexture.Sample(u_FrozenTexture_sampler, PSIn.UV);
  float4 currentFrag = u_CurrentTexture.Sample(u_CurrentTexture_sampler, PSIn.UV);
  PSOut.Color = lerp(frozenFrag, currentFrag, PSIn.Color.a);
}

)";

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

const base::String kHLSL_MappingTransitionRender = R"(

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = VSIn.Pos;
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

Texture2D u_FrozenTexture;
SamplerState u_FrozenTexture_sampler;

Texture2D u_CurrentTexture;
SamplerState u_CurrentTexture_sampler;

Texture2D u_TransTexture;
SamplerState u_TransTexture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frozenFrag = u_FrozenTexture.Sample(u_FrozenTexture_sampler, PSIn.UV);
  float4 currentFrag = u_CurrentTexture.Sample(u_CurrentTexture_sampler, PSIn.UV);
  float transSample = u_TransTexture.Sample(u_TransTexture_sampler, PSIn.UV).r;

  float vague = PSIn.Color.r;
  float progress = PSIn.Color.a;

  transSample = clamp(transSample, progress, progress + vague);
  float mixAlpha = (transSample - progress) / vague;

  PSOut.Color = lerp(currentFrag, frozenFrag, mixAlpha);
}

)";

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

const base::String kHLSL_TilemapRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct TilemapParams {
  float4 OffsetAndTexSize;
  float4 AnimateIndexAndTileSize;
};

cbuffer TilemapUniformBuffer {
  TilemapParams u_Params;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

static const float2 kAutotileArea = float2(3.0, 28.0);

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  float4 transPos = VSIn.Pos;
  float2 transUV = VSIn.UV;

  // Apply offset
  transPos.x += u_Params.OffsetAndTexSize.x;
  transPos.y += u_Params.OffsetAndTexSize.y;

  // Animated area
  float tile_size = u_Params.AnimateIndexAndTileSize.y;
  float addition = (transUV.x <= kAutotileArea.x * tile_size &&
                    transUV.y <= kAutotileArea.y * tile_size)
                       ? 1.0
                       : 0.0;
  transUV.x += 3.0 * tile_size * u_Params.AnimateIndexAndTileSize.x * addition;

  // Setup pixel shader params
  PSIn.Pos = mul(u_Transform.ProjMat, transPos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = float2(transUV.x * u_Params.OffsetAndTexSize.z,
                   transUV.y * u_Params.OffsetAndTexSize.w);
  PSIn.Color = VSIn.Color;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  PSOut.Color.rgb = lerp(frag.rgb, PSIn.Color.rgb, PSIn.Color.a);
  PSOut.Color.a = frag.a;
}

)";

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

const base::String kHLSL_Tilemap2Render = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct Tilemap2Params {
  float4 OffsetAndTexSize;
  float4 AnimationOffsetAndTileSize;
};

cbuffer Tilemap2UniformBuffer {
  Tilemap2Params u_Params;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

float PosInArea(float2 pos, float4 area) {
  return (pos.x >= area.x && pos.y >= area.y && pos.x <= (area.x + area.z) &&
          pos.y <= (area.y + area.w))
             ? 1.0
             : 0.0;
}

static const float2 kRegularArea = float2(12.0, 12.0);
static const float4 kWaterfallArea = float4(12.0, 0.0, 4.0, 12.0);
static const float4 kWaterfallAutotileArea = float4(12.0, 0.0, 2.0, 6.0);

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  float4 transPos = VSIn.Pos;
  float2 transUV = VSIn.UV;

  // Apply offset
  transPos.x += u_Params.OffsetAndTexSize.x;
  transPos.y += u_Params.OffsetAndTexSize.y;

  // Regular area
  float tile_size = u_Params.AnimationOffsetAndTileSize.z;
	float addition1 = (transUV.x <= kRegularArea.x * tile_size &&
                     transUV.y <= kRegularArea.y * tile_size)
                        ? 1.0
                        : 0.0;
	transUV.x += u_Params.AnimationOffsetAndTileSize.x * addition1;

	// Waterfall area
	float addition2 = PosInArea(transUV, kWaterfallArea * tile_size) -
                    PosInArea(transUV, kWaterfallAutotileArea * tile_size);
	transUV.y += u_Params.AnimationOffsetAndTileSize.y * addition2;

  // Setup pixel shader params
  PSIn.Pos = mul(u_Transform.ProjMat, transPos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = float2(transUV.x * u_Params.OffsetAndTexSize.z,
                   transUV.y * u_Params.OffsetAndTexSize.w);
  PSIn.Color = VSIn.Color;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  PSOut.Color.rgb = lerp(frag.rgb, PSIn.Color.rgb, PSIn.Color.a);
  PSOut.Color.a = frag.a;
}

)";

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

const base::String kHLSL_BitmapHueRender = R"(

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = VSIn.Pos;
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

float3 rgb2hsv(float3 c) {
  float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  float4 p = lerp(float4(c.b, c.g, K.w, K.z), float4(c.g, c.b, K.x, K.y), step(c.b, c.g));
  float4 q = lerp(float4(p.x, p.y, p.w, c.r), float4(c.r, p.y, p.z, p.x), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1e-10;

  float h = abs(q.z + (q.w - q.y) / (6.0 * d + e));
  float s = d / (q.x + e);
  float v = q.x;

  return float3(h, s, v);
}

float3 hsv2rgb(float3 c) {
  float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  float3 t = float3(c.x, c.x, c.x) + K.xyz;
  float3 p = abs(frac(t) * 6.0 - float3(K.w, K.w, K.w));

  return c.z * lerp(float3(K.x, K.x, K.x), clamp(p - float3(K.x, K.x, K.x), 0.0, 1.0), c.y);
}

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  float3 hsv = rgb2hsv(frag.rgb);
  hsv.x += PSIn.Color.a;
  PSOut.Color = float4(hsv2rgb(hsv), frag.a);
}

)";

///
// type:
//   spine2d shader
///
// entry:
//   vertex: VSMain
//   pixel: PSMain
///
// vertex:
//   <float4, float4, float4>
///
// resource:
//   { float4x4, float4x4 }
//   { Texture2D }
///
const base::String kHLSL_Spine2DRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float2 Pos : ATTRIB0;
  float4 LightColor : ATTRIB1;
  float2 UV : ATTRIB2;
  float4 DarkColor : ATTRIB3;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 LightColor : COLOR0;
  float4 DarkColor : COLOR1;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  float4 pos = float4(VSIn.Pos.x, VSIn.Pos.y, 0.0, 1.0);
  PSIn.Pos = mul(u_Transform.ProjMat, pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.LightColor = VSIn.LightColor;
  PSIn.DarkColor = VSIn.DarkColor;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 texColor = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  float alpha = texColor.a * PSIn.LightColor.a;
  PSOut.Color.a = alpha;
  PSOut.Color.rgb = ((texColor.a - 1.0) * PSIn.DarkColor.a + 1.0 - texColor.rgb) * PSIn.DarkColor.rgb + texColor.rgb * PSIn.LightColor.rgb;
}

)";

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

const base::String kHLSL_YUVRender = R"(

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = VSIn.Pos;
  URGE_NDC_PROCESS(PSIn.Pos);
  PSIn.UV = VSIn.UV;
}

Texture2D u_TextureY;
SamplerState u_TextureY_sampler;

Texture2D u_TextureU;
SamplerState u_TextureU_sampler;

Texture2D u_TextureV;
SamplerState u_TextureV_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

static const float3 kYUVOffset = float3(0.0, -0.501960814, -0.501960814);
static const float3x3 kYUVMatrix = float3x3(1,       1,        1,
                                            0,      -0.3441,   1.772,
                                            1.402,  -0.7141,   0);

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float3 yuv;
  yuv.x = u_TextureY.Sample(u_TextureY_sampler, PSIn.UV).r;
  yuv.y = u_TextureU.Sample(u_TextureU_sampler, PSIn.UV).r;
  yuv.z = u_TextureV.Sample(u_TextureV_sampler, PSIn.UV).r;
  yuv += kYUVOffset;
  float3 rgb =
#if defined(GLSL)
    mul(kYUVMatrix, yuv);
#else
    mul(yuv, kYUVMatrix);
#endif
  PSOut.Color = float4(rgb.x, rgb.y, rgb.z, 1.0);
}

)";

///
// type:
//   present shader
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
//
// macros:
//   CONVERT_PS_OUTPUT_TO_GAMMA
///

const base::String kHLSL_PresentRender = R"(

struct WorldMatrix {
  float4x4 ProjMat;
  float4x4 TransMat;
};

cbuffer WorldMatrixBuffer {
  WorldMatrix u_Transform;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_Position;
  float2 UV : TEXCOORD0;
  float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_Transform.ProjMat, VSIn.Pos);
  PSIn.Pos = mul(u_Transform.TransMat, PSIn.Pos);
  PSIn.UV = VSIn.UV;
  PSIn.Color = VSIn.Color;
}

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSOutput {
  float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  frag.a *= PSIn.Color.a;
  SRGBA_TO_LINEAR(frag);
  PSOut.Color = frag;
}

)";

}  // namespace renderer
