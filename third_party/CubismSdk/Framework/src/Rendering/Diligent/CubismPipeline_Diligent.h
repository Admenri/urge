/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions (c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "Math/CubismVector2.hpp"
#include "Type/csmMap.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmVector.hpp"

#include "renderer/device/render_device.h"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

enum ShaderNames {
  // SetupMask
  ShaderNames_SetupMask,

  // Normal
  ShaderNames_Normal,
  ShaderNames_NormalMasked,
  ShaderNames_NormalMaskedInverted,
  ShaderNames_NormalPremultipliedAlpha,
  ShaderNames_NormalMaskedPremultipliedAlpha,
  ShaderNames_NormalMaskedInvertedPremultipliedAlpha,

  // Add
  ShaderNames_Add,
  ShaderNames_AddMasked,
  ShaderNames_AddMaskedInverted,
  ShaderNames_AddPremultipliedAlpha,
  ShaderNames_AddMaskedPremultipliedAlpha,
  ShaderNames_AddMaskedPremultipliedAlphaInverted,

  // Mult
  ShaderNames_Mult,
  ShaderNames_MultMasked,
  ShaderNames_MultMaskedInverted,
  ShaderNames_MultPremultipliedAlpha,
  ShaderNames_MultMaskedPremultipliedAlpha,
  ShaderNames_MultMaskedPremultipliedAlphaInverted,
};

enum Blend {
  Blend_Normal,
  Blend_Add,
  Blend_Mult,
  Blend_Mask,
};

class CubismPipeline_Diligent {
 public:
  CubismPipeline_Diligent(Diligent::IRenderDevice* device);
  ~CubismPipeline_Diligent();

  CubismPipeline_Diligent(const CubismPipeline_Diligent&) = delete;
  CubismPipeline_Diligent& operator=(const CubismPipeline_Diligent&) = delete;

  Diligent::IPipelineResourceSignature* GetSignature() const {
    return _signature;
  }

  Diligent::IPipelineState* GetPipeline(csmInt32 vertIndex,
                                        csmInt32 pixelIndex,
                                        csmInt32 blendIndex,
                                        csmBool culling) {
    return _pipelines[vertIndex][pixelIndex][blendIndex][!!culling];
  }

 private:
  void MakePipelineStates();
  void CreateShader(Diligent::SHADER_TYPE type,
                    const csmChar* entry,
                    Diligent::IShader** shader);

  Diligent::IRenderDevice* _device;
  RRefPtr<Diligent::IPipelineResourceSignature> _signature;
  // vert, pixel, blend, cull
  RRefPtr<Diligent::IPipelineState> _pipelines[3][7][4][2];
};

class CubismShaderBinding_Diligent {
 public:
  CubismShaderBinding_Diligent();

  void InitializeBinding(Diligent::IPipelineResourceSignature* signature);

  void SetConstantBuffer(Diligent::IBuffer* constantBuffer);
  void SetMainTexture(Diligent::ITextureView* shaderResourceView);
  void SetMaskTexture(Diligent::ITextureView* shaderResourceView);

  Diligent::IShaderResourceBinding* GetBinding() const { return _binding; }

 private:
  RRefPtr<Diligent::IShaderResourceBinding> _binding;

  RRefPtr<Diligent::IShaderResourceVariable> _constantBufferVert;
  RRefPtr<Diligent::IShaderResourceVariable> _constantBufferPixel;
  RRefPtr<Diligent::IShaderResourceVariable> _mainTexture;
  RRefPtr<Diligent::IShaderResourceVariable> _maskTexture;
};

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
