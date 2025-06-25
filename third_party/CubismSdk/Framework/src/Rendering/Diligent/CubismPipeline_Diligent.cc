#include "CubismPipeline_Diligent.h"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

const char kCubismEffectHLSL[] = R"(
cbuffer CubismConstants {
  float4x4 projectMatrix;
  float4x4 clipMatrix;
  float4 baseColor;
  float4 multiplyColor;
  float4 screenColor;
  float4 channelFlag;
}

Texture2D mainTexture;
SamplerState mainTexture_sampler;
Texture2D maskTexture;
SamplerState maskTexture_sampler;

// Vertex shader input
struct VS_IN {
  float2 pos : ATTRIB0;
  float2 uv : ATTRIB1;
};

// Vertex shader output
struct VS_OUT {
  float4 Position : SV_POSITION;
  float2 uv : TEXCOORD0;
  float4 clipPosition : TEXCOORD1;
};

// Mask shader
VS_OUT VertSetupMask(VS_IN In) {
  VS_OUT Out;
  Out.Position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
  Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
  Out.uv.x = In.uv.x;
  Out.uv.y = 1.0f - In.uv.y;
  return Out;
}

float4 PixelSetupMask(VS_OUT In) : SV_Target {
  float isInside = step(baseColor.x, In.clipPosition.x / In.clipPosition.w) *
                   step(baseColor.y, In.clipPosition.y / In.clipPosition.w) *
                   step(In.clipPosition.x / In.clipPosition.w, baseColor.z) *
                   step(In.clipPosition.y / In.clipPosition.w, baseColor.w);
  return channelFlag * mainTexture.Sample(mainTexture_sampler, In.uv).a *
         isInside;
}

// Vertex shader
// normal
VS_OUT VertNormal(VS_IN In) {
  VS_OUT Out;
  Out.Position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
  Out.clipPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
  Out.uv.x = In.uv.x;
  Out.uv.y = 1.0f - In.uv.y;
  return Out;
}

// masked
VS_OUT VertMasked(VS_IN In) {
  VS_OUT Out;
  Out.Position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
  Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), clipMatrix);
  Out.uv.x = In.uv.x;
  Out.uv.y = 1.0f - In.uv.y;
  return Out;
}

// Pixel Shader
// normal
float4 PixelNormal(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb =
      (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  color.xyz *= color.w;
  return color;
}

// normal premult alpha
float4 PixelNormalPremult(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) -
                 (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  return color;
}

// masked
float4 PixelMasked(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb =
      (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  color.xyz *= color.w;
  float4 clipMask =
      (1.0f - maskTexture.Sample(maskTexture_sampler,
                                 In.clipPosition.xy / In.clipPosition.w)) *
      channelFlag;
  float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
  color = color * maskVal;
  return color;
}

// masked inverted
float4 PixelMaskedInverted(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb =
      (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  color.xyz *= color.w;
  float4 clipMask =
      (1.0f - maskTexture.Sample(maskTexture_sampler,
                                 In.clipPosition.xy / In.clipPosition.w)) *
      channelFlag;
  float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
  color = color * (1.0f - maskVal);
  return color;
}

// masked premult alpha
float4 PixelMaskedPremult(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) -
                 (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  float4 clipMask =
      (1.0f - maskTexture.Sample(maskTexture_sampler,
                                 In.clipPosition.xy / In.clipPosition.w)) *
      channelFlag;
  float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
  color = color * maskVal;
  return color;
}

// masked inverted premult alpha
float4 PixelMaskedInvertedPremult(VS_OUT In) : SV_Target {
  float4 texColor = mainTexture.Sample(mainTexture_sampler, In.uv);
  texColor.rgb = texColor.rgb * multiplyColor.rgb;
  texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) -
                 (texColor.rgb * screenColor.rgb);
  float4 color = texColor * baseColor;
  float4 clipMask =
      (1.0f - maskTexture.Sample(maskTexture_sampler,
                                 In.clipPosition.xy / In.clipPosition.w)) *
      channelFlag;
  float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
  color = color * (1.0f - maskVal);
  return color;
}

)";

CubismPipeline_Diligent::CubismPipeline_Diligent(
    Diligent::IRenderDevice* device)
    : _device(device) {
  MakePipelineStates();
}

CubismPipeline_Diligent::~CubismPipeline_Diligent() = default;

void CubismPipeline_Diligent::MakePipelineStates() {
  // Shaders
  RRefPtr<Diligent::IShader> vertShaders[3];
  CreateShader(Diligent::SHADER_TYPE_VERTEX, "VertSetupMask",
               &vertShaders[ShaderNames_SetupMask]);
  CreateShader(Diligent::SHADER_TYPE_VERTEX, "VertNormal",
               &vertShaders[ShaderNames_Normal]);
  CreateShader(Diligent::SHADER_TYPE_VERTEX, "VertMasked",
               &vertShaders[ShaderNames_NormalMasked]);

  RRefPtr<Diligent::IShader> pixelShaders[7];
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelSetupMask",
               &pixelShaders[ShaderNames_SetupMask]);
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelNormal",
               &pixelShaders[ShaderNames_Normal]);
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelMasked",
               &pixelShaders[ShaderNames_NormalMasked]);
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelMaskedInverted",
               &pixelShaders[ShaderNames_NormalMaskedInverted]);
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelNormalPremult",
               &pixelShaders[ShaderNames_NormalPremultipliedAlpha]);
  CreateShader(Diligent::SHADER_TYPE_PIXEL, "PixelMaskedPremult",
               &pixelShaders[ShaderNames_NormalMaskedPremultipliedAlpha]);
  CreateShader(
      Diligent::SHADER_TYPE_PIXEL, "PixelMaskedInvertedPremult",
      &pixelShaders[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]);

  // Pipeline Signature
  Diligent::PipelineResourceDesc pipelineResources[] = {
      {Diligent::SHADER_TYPE_VERTEX | Diligent::SHADER_TYPE_PIXEL,
       "CubismConstants", Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "mainTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "maskTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  Diligent::ImmutableSamplerDesc immutableSamplers[] = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "mainTexture",
          {Diligent::FILTER_TYPE_POINT, Diligent::FILTER_TYPE_POINT,
           Diligent::FILTER_TYPE_POINT, Diligent::TEXTURE_ADDRESS_CLAMP,
           Diligent::TEXTURE_ADDRESS_CLAMP, Diligent::TEXTURE_ADDRESS_CLAMP},
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "maskTexture",
          {Diligent::FILTER_TYPE_POINT, Diligent::FILTER_TYPE_POINT,
           Diligent::FILTER_TYPE_POINT, Diligent::TEXTURE_ADDRESS_CLAMP,
           Diligent::TEXTURE_ADDRESS_CLAMP, Diligent::TEXTURE_ADDRESS_CLAMP},
      },
  };

  Diligent::PipelineResourceSignatureDesc signatureDesc;
  signatureDesc.Resources = pipelineResources;
  signatureDesc.NumResources = std::size(pipelineResources);
  signatureDesc.ImmutableSamplers = immutableSamplers;
  signatureDesc.NumImmutableSamplers = std::size(immutableSamplers);
  signatureDesc.UseCombinedTextureSamplers = Diligent::True;
  _device->CreatePipelineResourceSignature(signatureDesc, &_signature);

  // Pipeline States
  Diligent::LayoutElement input_layout[] = {
      /* Position Vec2 */
      Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
      /* TexCoord Vec2 */
      Diligent::LayoutElement{1, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
  };

  Diligent::GraphicsPipelineStateCreateInfo pipelineCreateInfo;
  pipelineCreateInfo.ppResourceSignatures = &_signature;
  pipelineCreateInfo.ResourceSignaturesCount = 1;

  pipelineCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = input_layout;
  pipelineCreateInfo.GraphicsPipeline.InputLayout.NumElements =
      std::size(input_layout);

  pipelineCreateInfo.GraphicsPipeline.RasterizerDesc.ScissorEnable =
      Diligent::True;
  pipelineCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  pipelineCreateInfo.GraphicsPipeline.RTVFormats[0] =
      Diligent::TEX_FORMAT_RGBA8_UNORM;
  pipelineCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable =
      Diligent::False;

  Diligent::RenderTargetBlendDesc renderTargetBlends[] = {
      // Normal
      Diligent::RenderTargetBlendDesc{
          Diligent::True,
          Diligent::False,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_FACTOR_INV_SRC_ALPHA,
          Diligent::BLEND_OPERATION_ADD,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_FACTOR_INV_SRC_ALPHA,
          Diligent::BLEND_OPERATION_ADD,
      },
      // Add
      Diligent::RenderTargetBlendDesc{
          Diligent::True,
          Diligent::False,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_OPERATION_ADD,
          Diligent::BLEND_FACTOR_ZERO,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_OPERATION_ADD,
      },
      // Mult
      Diligent::RenderTargetBlendDesc{
          Diligent::True,
          Diligent::False,
          Diligent::BLEND_FACTOR_DEST_COLOR,
          Diligent::BLEND_FACTOR_INV_SRC_ALPHA,
          Diligent::BLEND_OPERATION_ADD,
          Diligent::BLEND_FACTOR_ZERO,
          Diligent::BLEND_FACTOR_ONE,
          Diligent::BLEND_OPERATION_ADD,
      },
      // Mask
      Diligent::RenderTargetBlendDesc{
          Diligent::True,
          Diligent::False,
          Diligent::BLEND_FACTOR_ZERO,
          Diligent::BLEND_FACTOR_INV_SRC_COLOR,
          Diligent::BLEND_OPERATION_ADD,
          Diligent::BLEND_FACTOR_ZERO,
          Diligent::BLEND_FACTOR_INV_SRC_ALPHA,
          Diligent::BLEND_OPERATION_ADD,
      },
  };

  for (int32_t vertIndex = 0; vertIndex < 3; ++vertIndex) {
    for (int32_t pixelIndex = 0; pixelIndex < 7; ++pixelIndex) {
      for (int32_t blendIndex = 0; blendIndex < 4; ++blendIndex) {
        for (int32_t cullIndex = 0; cullIndex < 2; ++cullIndex) {
          pipelineCreateInfo.PSODesc.Name = "cubism.pipeline.collection";
          pipelineCreateInfo.pVS = vertShaders[vertIndex];
          pipelineCreateInfo.pPS = pixelShaders[pixelIndex];
          pipelineCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0] =
              renderTargetBlends[blendIndex];
          pipelineCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode =
              cullIndex ? Diligent::CULL_MODE_BACK : Diligent::CULL_MODE_NONE;

          _device->CreatePipelineState(
              pipelineCreateInfo,
              &_pipelines[vertIndex][pixelIndex][blendIndex][cullIndex]);
        }
      }
    }
  }
}

void CubismPipeline_Diligent::CreateShader(Diligent::SHADER_TYPE type,
                                           const csmChar* entry,
                                           Diligent::IShader** shader) {
  Diligent::ShaderCreateInfo shaderCreateInfo;
  shaderCreateInfo.Source = kCubismEffectHLSL;
  shaderCreateInfo.SourceLength = std::strlen(kCubismEffectHLSL);
  shaderCreateInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  shaderCreateInfo.EntryPoint = entry;
  shaderCreateInfo.CompileFlags =
      Diligent::SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
  shaderCreateInfo.Desc.Name = entry;
  shaderCreateInfo.Desc.ShaderType = type;
  shaderCreateInfo.Desc.UseCombinedTextureSamplers = Diligent::True;
  _device->CreateShader(shaderCreateInfo, shader);
}

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
