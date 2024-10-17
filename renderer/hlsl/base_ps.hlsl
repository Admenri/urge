
Texture2D u_Texture;
SamplerState u_Texture_sampler;

struct PSInput {
  float4 Pos : SV_POSITION;
  float2 UV : TEX_COORD;
};

struct PSOutput {
  float4 Color : SV_TARGET;
};

void main(in PSInput PSIn, out PSOutput PSOut) {
  PSOut.Color = u_Texture.Sample(u_Texture_sampler, PSIn.UV);
}
