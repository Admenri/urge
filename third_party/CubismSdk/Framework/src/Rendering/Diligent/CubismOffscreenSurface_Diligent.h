#pragma once

#include "CubismFramework.hpp"

#include "renderer/context/render_context.h"
#include "renderer/device/render_device.h"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

class CubismOffscreenSurface_Diligent {
 public:
  CubismOffscreenSurface_Diligent();

  void BeginDraw(renderer::RenderContext* renderContext);
  void EndDraw(renderer::RenderContext* renderContext);
  void Clear(renderer::RenderContext* renderContext,
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
