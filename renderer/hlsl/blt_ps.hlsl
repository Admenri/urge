
cbuffer PSConstants {
  float4 u_OffsetScale;
  float u_Opacity;
};

Texture2D u_Texture;
SamplerState u_Texture_sampler;

Texture2D u_DstTexture;
SamplerState u_DstTexture_sampler;

struct PSInput {
  float4 Pos : SV_POSITION;
  float2 UV : TEX_COORD;
};

struct PSOutput {
  float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut) {
  float2 dstTexCoord = (PSIn.UV - u_OffsetScale.xy) * u_OffsetScale.zw;

  float4 srcFrag = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
  float4 dstFrag = u_DstTexture.Sample(u_DstTexture_sampler, dstTexCoord);

  float4 resultFrag;

  float srcAlpha = srcFrag.a * u_Opacity;
  float dstAlpha = dstFrag.a * (1.0 - srcAlpha);
  resultFrag.a = srcAlpha + dstAlpha;

  if (resultFrag.a == 0.0)
    resultFrag.rgb = srcFrag.rgb;
  else
    resultFrag.rgb =
        (srcAlpha * srcFrag.rgb + dstAlpha * dstFrag.rgb) / resultFrag.a;

  PSOut.Color = resultFrag;
}
