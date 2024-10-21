
cbuffer VSConstants {
  float4x4 u_ProjMat;
  float2 u_TransOffset;
  float2 u_TexSize;
  float u_AnimateIndex;
  float u_TileSize;
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

static const float2 kAutotileArea = float2(3.0, 28.0);

void main(in VSInput VSIn, out PSInput PSIn) {
  float2 tex = VSIn.Pos.xy;

  // Animated area
  float addition = (tex.x <= kAutotileArea.x * u_TileSize &&
                    tex.y <= kAutotileArea.y * u_TileSize)
                       ? 1.0
                       : 0.0;
  tex.x += 3.0 * u_TileSize * u_AnimateIndex * addition;

  PSIn.Pos = mul(u_ProjMat, VSIn.Pos + float4(u_TransOffset, 0.0, 0.0));
  PSIn.UV = float2(tex.x * VSIn.UV.x, tex.y * VSIn.UV.y);
}
