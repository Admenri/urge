
cbuffer VSConstants {
  float4x4 u_ProjMat;
  float2 u_TransOffset;
};

struct VSInput {
  float4 Pos : ATTRIB0;
  float2 UV : ATTRIB1;
  float4 Color : ATTRIB2;
};

struct PSInput {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR;
};

void main(in VSInput VSIn, out PSInput PSIn) {
  PSIn.Pos = mul(u_ProjMat, VSIn.Pos + float4(u_TransOffset, 0.0, 0.0));
  PSIn.Color = VSIn.Color;
}
