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

#include "renderer/device/render_device.h"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

class CubismOffscreenSurface_Diligent {
 public:
  CubismOffscreenSurface_Diligent();

  void BeginDraw(Diligent::IDeviceContext* renderContext);
  void EndDraw(Diligent::IDeviceContext* renderContext);
  void Clear(Diligent::IDeviceContext* renderContext,
             float r,
             float g,
             float b,
             float a);

  csmBool CreateOffscreenSurface(renderer::RenderDevice* device,
                                 csmUint32 displayBufferWidth,
                                 csmUint32 displayBufferHeight);
  void DestroyOffscreenSurface();

  Diligent::ITextureView* GetTextureView() const;

  csmUint32 GetBufferWidth() const;
  csmUint32 GetBufferHeight() const;
  csmBool IsValid() const;

 private:
  RRefPtr<Diligent::ITexture> _texture;
  RRefPtr<Diligent::ITextureView> _textureView;
  RRefPtr<Diligent::ITextureView> _renderTargetView;

  csmUint32 _bufferWidth;
  csmUint32 _bufferHeight;
};

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
