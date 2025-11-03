/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  Copyright 2018-2025 Admenri Adev
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including
 * negligence), contract, or otherwise, unless required by applicable law (such
 * as deliberate and grossly negligent acts) or agreed to in writing, shall any
 * Contributor be liable for any damages, including any direct, indirect,
 * special, incidental, or consequential damages of any character arising as a
 * result of this License or out of the use or inability to use the software
 * (including but not limited to damages for loss of goodwill, work stoppage,
 * computer failure or malfunction, or any and all other commercial damages or
 * losses), even if such Contributor has been advised of the possibility of such
 * damages.
 */

#ifndef IMGUI_BACKENDS_IMGUI_IMPL_DILIGENT_H_
#define IMGUI_BACKENDS_IMGUI_IMPL_DILIGENT_H_
#pragma once

#include <memory>

#include "imgui.h"

#include "Common/interface/BasicMath.hpp"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Primitives/interface/BasicTypes.h"

struct ImDrawData;

namespace Diligent {

struct IRenderDevice;
struct IDeviceContext;
struct IBuffer;
struct IPipelineState;
struct ITextureView;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
struct ImGuiDiligentCreateInfo;
enum TEXTURE_FORMAT : Uint16;
enum SURFACE_TRANSFORM : Uint32;

/// Conversion mode to apply to imgui colors.
///
/// Imgui colors are defined in sRGB space.
/// Depending on the use case, they may need
/// to be converted to linear space.
enum IMGUI_COLOR_CONVERSION_MODE : Uint8 {
  /// Select the color conversion mode automatically:
  /// * Use SRGB_TO_LINEAR mode for sRGB framebuffers
  /// * Use NONE mode for non-sRGB framebuffers
  IMGUI_COLOR_CONVERSION_MODE_AUTO = 0,

  /// Always perform srgb-to-linear conversion.
  IMGUI_COLOR_CONVERSION_MODE_SRGB_TO_LINEAR,

  /// Do not perform any color conversion.
  IMGUI_COLOR_CONVERSION_MODE_NONE
};

struct ImGuiDiligentCreateInfo {
  static constexpr Uint32 DefaultInitialVBSize = 1024;
  static constexpr Uint32 DefaultInitialIBSize = 2048;

  IRenderDevice* pDevice = nullptr;

  TEXTURE_FORMAT BackBufferFmt = {};
  TEXTURE_FORMAT DepthBufferFmt = {};

  IMGUI_COLOR_CONVERSION_MODE ColorConversion =
      IMGUI_COLOR_CONVERSION_MODE_AUTO;

  FILTER_TYPE MinFilter = {};
  FILTER_TYPE MagFilter = {};
  FILTER_TYPE MipFilter = {};

  Uint32 InitialVertexBufferSize = DefaultInitialVBSize;
  Uint32 InitialIndexBufferSize = DefaultInitialIBSize;

  ImGuiDiligentCreateInfo() noexcept {}
  ImGuiDiligentCreateInfo(IRenderDevice* _pDevice,
                          TEXTURE_FORMAT _BackBufferFmt,
                          TEXTURE_FORMAT _DepthBufferFmt) noexcept;
  ImGuiDiligentCreateInfo(IRenderDevice* _pDevice,
                          const SwapChainDesc& _SCDesc) noexcept;
};

class ImGuiDiligentRenderer {
 public:
  ImGuiDiligentRenderer(const ImGuiDiligentCreateInfo& CI);
  ~ImGuiDiligentRenderer();

  void NewFrame(Uint32 RenderSurfaceWidth,
                Uint32 RenderSurfaceHeight,
                SURFACE_TRANSFORM SurfacePreTransform);
  void EndFrame();
  void RenderDrawData(IDeviceContext* pCtx, ImDrawData* pDrawData);
  void InvalidateDeviceObjects();
  void CreateDeviceObjects();

 private:
  inline float4 TransformClipRect(const ImVec2& DisplaySize,
                                  const float4& rect) const;
  void UpdateTexture(IDeviceContext* pCtx, ImTextureData* tex);
  void DestroyTexture(ImTextureData* tex);

 private:
  RefCntAutoPtr<IRenderDevice> m_pDevice;
  RefCntAutoPtr<IBuffer> m_pVB;
  RefCntAutoPtr<IBuffer> m_pIB;
  RefCntAutoPtr<IBuffer> m_pVertexConstantBuffer;
  RefCntAutoPtr<IPipelineState> m_pPSO;
  RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
  IShaderResourceVariable* m_pTextureVar = nullptr;

  const TEXTURE_FORMAT m_BackBufferFmt;
  const TEXTURE_FORMAT m_DepthBufferFmt;
  const FILTER_TYPE m_MinFilter;
  const FILTER_TYPE m_MagFilter;
  const FILTER_TYPE m_MipFilter;
  Uint32 m_VertexBufferSize = 0;
  Uint32 m_IndexBufferSize = 0;
  Uint32 m_RenderSurfaceWidth = 0;
  Uint32 m_RenderSurfaceHeight = 0;
  SURFACE_TRANSFORM m_SurfacePreTransform = SURFACE_TRANSFORM_IDENTITY;
  const IMGUI_COLOR_CONVERSION_MODE m_ColorConversionMode;
  bool m_BaseVertexSupported = false;
};

}  // namespace Diligent

#endif  //! IMGUI_BACKENDS_IMGUI_IMPL_DILIGENT_H_
