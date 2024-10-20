
cbuffer PSConstants {
  float u_Progress;
  float u_Vague;
};

Texture2D u_FrozenTexture;
SamplerState u_FrozenTexture_sampler;

Texture2D u_CurrentTexture;
SamplerState u_CurrentTexture_sampler;

Texture2D u_TransTexture;
SamplerState u_TransTexture_sampler;

struct PSInput {
  float4 Pos : SV_POSITION;
  float2 UV : TEX_COORD;
};

struct PSOutput {
  float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut) {
  float4 frozenFrag = u_FrozenTexture.Sample(u_FrozenTexture_sampler, PSIn.UV);
  float4 currentFrag =
      u_CurrentTexture.Sample(u_CurrentTexture_sampler, PSIn.UV);
  float transSample = u_TransTexture.Sample(u_TransTexture_sampler, PSIn.UV).r;

  transSample = clamp(transSample, u_Progress, u_Progress + u_Vague);
  float mixAlpha = (transSample - u_Progress) / u_Vague;

  PSOut.Color = lerp(currentFrag, frozenFrag, mixAlpha);
}
