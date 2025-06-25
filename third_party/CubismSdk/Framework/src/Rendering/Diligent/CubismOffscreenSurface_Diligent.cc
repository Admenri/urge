/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions (c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenSurface_Diligent.h"

#include "Utils/CubismDebug.hpp"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

namespace {

Diligent::ITextureView* s_backRenderTargetView = nullptr;
Diligent::ITextureView* s_backDepthView = nullptr;

}  // namespace

CubismOffscreenSurface_Diligent::CubismOffscreenSurface_Diligent()
    : _bufferWidth(0), _bufferHeight(0) {}

void CubismOffscreenSurface_Diligent::BeginDraw(
    renderer::RenderContext* renderContext) {
  if (!_textureView || !_renderTargetView) {
    return;
  }

  (*renderContext)
      ->SetRenderTargets(1, &_renderTargetView, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  renderContext->ScissorState()->Apply(
      base::Rect(0, 0, _bufferWidth, _bufferHeight));
}

void CubismOffscreenSurface_Diligent::EndDraw(
    renderer::RenderContext* renderContext) {
  (*renderContext)
      ->SetRenderTargets(0, nullptr, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void CubismOffscreenSurface_Diligent::Clear(
    renderer::RenderContext* renderContext,
    float r,
    float g,
    float b,
    float a) {
  float clearColor[4] = {r, g, b, a};
  (*renderContext)
      ->ClearRenderTarget(_renderTargetView, clearColor,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

csmBool CubismOffscreenSurface_Diligent::CreateOffscreenSurface(
    renderer::RenderDevice* device,
    csmUint32 displayBufferWidth,
    csmUint32 displayBufferHeight) {
  DestroyOffscreenSurface();

  do {
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "cubism.framework.offscreen.canvas";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.BindFlags =
        Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    textureDesc.Width = displayBufferWidth;
    textureDesc.Height = displayBufferHeight;
    (*device)->CreateTexture(textureDesc, nullptr, &_texture);
    if (!_texture) {
      CubismLogError("Error : create offscreen texture");
      break;
    }

    _renderTargetView =
        _texture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    _textureView =
        _texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);

    _bufferWidth = displayBufferWidth;
    _bufferHeight = displayBufferHeight;

    return true;

  } while (0);

  DestroyOffscreenSurface();

  return false;
}

void CubismOffscreenSurface_Diligent::DestroyOffscreenSurface() {
  _textureView.Release();
  _renderTargetView.Release();

  _texture.Release();
}

Diligent::ITextureView* CubismOffscreenSurface_Diligent::GetTextureView()
    const {
  return _textureView;
}

csmUint32 CubismOffscreenSurface_Diligent::GetBufferWidth() const {
  return _bufferWidth;
}

csmUint32 CubismOffscreenSurface_Diligent::GetBufferHeight() const {
  return _bufferHeight;
}

csmBool CubismOffscreenSurface_Diligent::IsValid() const {
  if (!_textureView || !_renderTargetView) {
    return false;
  }

  return true;
}

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
