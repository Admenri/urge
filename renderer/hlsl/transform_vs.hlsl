
cbuffer VSConstants {
  float4x4 u_ProjMat;
  float4x4 u_TransformMat;
  float2 u_TexSize;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_POSITION;
  float2 UV : TEX_COORD;
};

void main(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_ProjMat, mul(u_TransformMat, VSIn.Pos));
  PSIn.UV = float2(u_TexSize.x * VSIn.UV.x, u_TexSize.y * VSIn.UV.y);
}
