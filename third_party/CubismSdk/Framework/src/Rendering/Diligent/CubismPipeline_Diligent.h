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
                                        csmBool culling,
                                        csmBool enableDepth) {
    return _pipelines[vertIndex][pixelIndex][blendIndex][!!culling]
                     [enableDepth];
  }

 private:
  void MakePipelineStates();
  void CreateShader(Diligent::SHADER_TYPE type,
                    const csmChar* entry,
                    Diligent::IShader** shader);

  Diligent::IRenderDevice* _device;
  RRefPtr<Diligent::IPipelineResourceSignature> _signature;
  // vert, pixel, blend, cull, depth
  RRefPtr<Diligent::IPipelineState> _pipelines[3][7][4][2][2];
};

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
