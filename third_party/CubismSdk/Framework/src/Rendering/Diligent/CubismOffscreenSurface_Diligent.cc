#include "CubismOffscreenSurface_Diligent.h"

#include "Utils/CubismDebug.hpp"

namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

CubismOffscreenSurface_Diligent::CubismOffscreenSurface_Diligent()
    : _texture(nullptr),
      _textureView(nullptr),
      _renderTargetView(nullptr),
      _depthTexture(nullptr),
      _depthView(nullptr),
      _backupRender(nullptr),
      _backupDepth(nullptr),
      _bufferWidth(0),
      _bufferHeight(0) {}

void CubismOffscreenSurface_Diligent::BeginDraw(
    renderer::RenderContext* renderContext) {
  if (!_textureView || !_renderTargetView || !_depthView) {
    return;
  }

  _backupRender = nullptr;
  _backupDepth = nullptr;

  renderContext->GetRenderTargets(1, &_backupRender, &_backupDepth);
  renderContext->SetRenderTargets(
      1, &_renderTargetView, _depthView,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void CubismOffscreenSurface_Diligent::EndDraw(
    renderer::RenderContext* renderContext) {
  if (!_textureView || !_renderTargetView || !_depthView) {
    return;
  }

  renderContext->SetRenderTargets(
      1, &_backupRender, _backupDepth,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  _backupDepth = nullptr;
  _backupRender = nullptr;
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
  (*renderContext)
      ->ClearDepthStencil(_depthView, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

csmBool CubismOffscreenSurface_Diligent::CreateOffscreenSurface(
    renderer::RenderDevice* device,
    csmUint32 displayBufferWidth,
    csmUint32 displayBufferHeight) {
  DestroyOffscreenSurface();

  do {
    Diligent::TextureDesc textureDesc;
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

    // Depth/Stencil
    Diligent::TextureDesc depthDesc;
    depthDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    depthDesc.Width = displayBufferWidth;
    depthDesc.Height = displayBufferHeight;
    depthDesc.Format = Diligent::TEX_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
    (*device)->CreateTexture(depthDesc, nullptr, &_depthTexture);
    if (!_depthTexture) {
      CubismLogError("Error : create offscreen depth texture");
      break;
    }

    _depthView =
        _depthTexture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);

    _bufferWidth = displayBufferWidth;
    _bufferHeight = displayBufferHeight;

    return true;

  } while (0);

  DestroyOffscreenSurface();

  return false;
}

void CubismOffscreenSurface_Diligent::DestroyOffscreenSurface() {
  _backupDepth = nullptr;
  _backupRender = nullptr;
  _depthView = nullptr;
  _textureView = nullptr;
  _renderTargetView = nullptr;

  _depthTexture.Release();
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
  if (!_textureView || !_renderTargetView || !_depthView) {
    return false;
  }

  return true;
}

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
