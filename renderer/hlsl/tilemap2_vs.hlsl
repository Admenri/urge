
cbuffer VSConstants {
  float4x4 u_ProjMat;
  float2 u_TransOffset;
  float2 u_TexSize;
  float2 u_AutotileAnimationOffset;
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

static const float2 kRegularArea = float2(12.0, 12.0);
static const float4 kWaterfallArea = float4(12.0, 16.0, 4.0, 12.0);
static const float4 kWaterfallAutotileArea = float4(12.0, 16.0, 2.0, 6.0);

float posInArea(float2 pos, float4 area) {
  return (pos.x >= area.x && pos.y >= area.y && pos.x <= (area.x + area.z) &&
          pos.y <= (area.y + area.w))
             ? 1.0
             : 0.0;
}

void main(in VSInput VSIn, out PSInput PSIn) {
  float2 tex = VSIn.UV;
  float addition = 0.0;

  // Regular area
  addition = (tex.x <= kRegularArea.x * u_TileSize &&
              tex.y <= kRegularArea.y * u_TileSize)
                 ? 1.0
                 : 0.0;
  tex.x += u_AutotileAnimationOffset.x * addition;

  // Waterfall area
  addition =
      posInArea(tex, kWaterfallArea) - posInArea(tex, kWaterfallAutotileArea);
  tex.y += u_AutotileAnimationOffset.y * addition;

  PSIn.Pos = mul(u_ProjMat, VSIn.Pos + float4(u_TransOffset, 0.0, 0.0));
  PSIn.UV = float2(tex.x * u_TexSize.x, tex.y * u_TexSize.y);
}
