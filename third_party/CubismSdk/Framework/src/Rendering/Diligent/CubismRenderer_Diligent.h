/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions (c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../CubismClippingManager.hpp"
#include "../CubismRenderer.hpp"
#include "CubismFramework.hpp"
#include "CubismOffscreenSurface_Diligent.h"
#include "CubismPipeline_Diligent.h"
#include "Math/CubismVector2.hpp"
#include "Type/csmMap.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmVector.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D {
namespace Cubism {
namespace Framework {

struct CubismVertexDiligent {
  float x, y;  // Position
  float u, v;  // UVs
};

struct CubismConstantBufferDiligent {
  csmFloat32 projectMatrix[16];
  csmFloat32 clipMatrix[16];
  csmFloat32 baseColor[4];
  csmFloat32 multiplyColor[4];
  csmFloat32 screenColor[4];
  csmFloat32 channelFlag[4];
};

namespace Rendering {

class CubismRenderer_Diligent;
class CubismClippingContext_Diligent;

inline void ConvertToNative(CubismMatrix44& mtx, csmFloat32 out[16]) {
  std::memcpy(out, mtx.GetArray(), sizeof(csmFloat32) * 16);
}

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

class CubismClippingManager_Diligent
    : public CubismClippingManager<CubismClippingContext_Diligent,
                                   CubismOffscreenSurface_Diligent> {
 public:
  void SetupClippingContext(renderer::RenderDevice* device,
                            renderer::RenderContext* renderContext,
                            CubismModel& model,
                            CubismRenderer_Diligent* renderer,
                            csmInt32 offscreenCurrent);
};

class CubismClippingContext_Diligent : public CubismClippingContext {
  friend class CubismClippingManager_Diligent;
  friend class CubismRenderer_Diligent;

 public:
  CubismClippingContext_Diligent(
      CubismClippingManager<CubismClippingContext_Diligent,
                            CubismOffscreenSurface_Diligent>* manager,
      CubismModel& model,
      const csmInt32* clippingDrawableIndices,
      csmInt32 clipCount);

  virtual ~CubismClippingContext_Diligent();

  CubismClippingManager<CubismClippingContext_Diligent,
                        CubismOffscreenSurface_Diligent>*
  GetClippingManager();

  CubismClippingManager<CubismClippingContext_Diligent,
                        CubismOffscreenSurface_Diligent>* _owner;
};

class CubismRenderer_Diligent : public CubismRenderer {
  friend class CubismRenderer;
  friend class CubismClippingManager_Diligent;

 public:
  static void InitializeConstantSettings(csmUint32 bufferSetNum,
                                         renderer::RenderDevice* device);
  static void StartFrame(renderer::RenderDevice* device,
                         renderer::RenderContext* renderContext,
                         CubismOffscreenSurface_Diligent& renderTarget);
  static void EndFrame(renderer::RenderDevice* device);
  static CubismPipeline_Diligent* GetPipelineManager();
  static void DeletePipelineManager();
  static renderer::RenderDevice* GetCurrentDevice();

  virtual void Initialize(Framework::CubismModel* model);
  virtual void Initialize(Framework::CubismModel* model,
                          csmInt32 maskBufferCount);

  void BindTexture(csmUint32 modelTextureAssign,
                   Diligent::ITextureView* textureView);

  const csmMap<csmInt32, RRefPtr<Diligent::ITextureView>>& GetBindedTextures()
      const;
  void SetClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);
  csmInt32 GetRenderTextureCount() const;
  CubismVector2 GetClippingMaskBufferSize() const;
  CubismOffscreenSurface_Diligent* GetMaskBuffer(csmUint32 backbufferNum,
                                                 csmInt32 offscreenIndex);

 protected:
  CubismRenderer_Diligent();
  virtual ~CubismRenderer_Diligent();

  virtual void DoDrawModel() override;

  void DrawMeshDiligent(const CubismModel& model, const csmInt32 index);

 private:
  void ExecuteDrawForMask(const CubismModel& model, const csmInt32 index);
  void ExecuteDrawForDraw(const CubismModel& model, const csmInt32 index);
  void DrawDrawableIndexed(const CubismModel& model, const csmInt32 index);

  CubismRenderer_Diligent(const CubismRenderer_Diligent&) = delete;
  CubismRenderer_Diligent& operator=(const CubismRenderer_Diligent&) = delete;

  void PostDraw();

  virtual void SaveProfile() override;
  virtual void RestoreProfile() override;

  void SetClippingContextBufferForMask(CubismClippingContext_Diligent* clip);
  CubismClippingContext_Diligent* GetClippingContextBufferForMask() const;

  void SetClippingContextBufferForDraw(CubismClippingContext_Diligent* clip);
  CubismClippingContext_Diligent* GetClippingContextBufferForDraw() const;

  void CopyToBuffer(renderer::RenderContext* renderContext,
                    csmInt32 drawAssign,
                    const csmInt32 vcount,
                    const csmFloat32* varray,
                    const csmFloat32* uvarray);

  Diligent::ITextureView* GetTextureViewWithIndex(const CubismModel& model,
                                                  const csmInt32 index);

  Diligent::IPipelineState* DerivePipeline(const CubismModel& model,
                                           const csmInt32 index);
  void SetTextureView(const CubismModel& model,
                      const csmInt32 index,
                      Diligent::ITextureView** textureView,
                      Diligent::ITextureView** maskView);
  void SetColorConstantBuffer(CubismConstantBufferDiligent& cb,
                              const CubismModel& model,
                              const csmInt32 index,
                              CubismTextureColor& baseColor,
                              CubismTextureColor& multiplyColor,
                              CubismTextureColor& screenColor);
  void SetColorChannel(CubismConstantBufferDiligent& cb,
                       CubismClippingContext_Diligent* contextBuffer);
  void SetProjectionMatrix(CubismConstantBufferDiligent& cb,
                           CubismMatrix44 matrix);
  Diligent::IBuffer* UpdateConstantBuffer(CubismConstantBufferDiligent& cb,
                                          csmInt32 index);
  const csmBool inline IsGeneratingMask() const;

  Diligent::IBuffer*** _vertexBuffers;
  Diligent::IBuffer*** _indexBuffers;
  Diligent::IBuffer*** _constantBuffers;
  csmUint32 _drawableNum;

  csmVector<csmVector<CubismShaderBinding_Diligent>> _shaderBindings;

  csmInt32 _commandBufferNum;
  csmInt32 _commandBufferCurrent;

  csmVector<csmInt32> _sortedDrawableIndexList;

  csmMap<csmInt32, RRefPtr<Diligent::ITextureView>> _textures;

  csmVector<csmVector<CubismOffscreenSurface_Diligent>> _offscreenSurfaces;

  CubismClippingManager_Diligent* _clippingManager;
  CubismClippingContext_Diligent* _clippingContextBufferForMask;
  CubismClippingContext_Diligent* _clippingContextBufferForDraw;
};

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
//------------ LIVE2D NAMESPACE ------------
