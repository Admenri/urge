
cbuffer PSConstants {
  float4 u_Color;
  float4 u_Tone;
  float u_Opacity;
};

Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSInput {
  float4 Pos : SV_POSITION;
  float2 UV : TEX_COORD;
};

struct PSOutput {
  float4 Color : SV_TARGET;
};

static const float3 lumaF = float3(0.299, 0.587, 0.114);

void main(in PSInput PSIn, out PSOutput PSOut) {
  float4 frag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);

  /* Tone */
  float luma = dot(frag.rgb, lumaF);
  frag.rgb = lerp(frag.rgb, float3(luma, luma, luma), u_Tone.w);
  frag.rgb += u_Tone.rgb;

  /* Opacity */
  frag.a *= u_Opacity;

  /* Color */
  frag.rgb = lerp(frag.rgb, u_Color.rgb, u_Color.a);

  PSOut.Color = frag;
}
