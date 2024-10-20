// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/sprite.h"

#include <math.h>

#include "renderer/device/render_device.h"

#if !defined(M_PI)
#define M_PI 3.1415926
#endif

namespace content {

Sprite::Sprite(scoped_refptr<Graphics> screen, scoped_refptr<Viewport> viewport)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen, viewport) {
  InitAttributeInternal();
}

Sprite::~Sprite() {
  Dispose();
}

void Sprite::SetBitmap(scoped_refptr<Bitmap> bitmap) {
  CheckIsDisposed();

  if (bitmap == bitmap_)
    return;
  bitmap_ = bitmap;

  if (IsObjectValid(bitmap_.get())) {
    src_rect_->Set(bitmap_->GetSize());
    OnSrcRectChangedInternal();
  }
}

void Sprite::SetSrcRect(scoped_refptr<Rect> rect) {
  CheckIsDisposed();

  if (src_rect_->IsSame(*rect))
    return;

  *src_rect_ = *rect;
  OnSrcRectChangedInternal();
}

void Sprite::SetMirror(bool mirror) {
  CheckIsDisposed();

  if (mirror_ == mirror)
    return;

  mirror_ = mirror;
  OnSrcRectChangedInternal();
}

void Sprite::Update() {
  CheckIsDisposed();
  Flashable::Update();

  wave_.phase += wave_.speed / 180.0f;
  wave_.need_update = true;
}

void Sprite::InitAttributeInternal() {
  color_ = new Color();
  tone_ = new Tone();

  src_rect_ = new Rect();
  src_rect_observer_ = src_rect_->AddChangedObserver(base::BindRepeating(
      &Sprite::OnSrcRectChangedInternal, base::Unretained(this)));

  drawable_quad_ = std::make_unique<renderer::QuadDrawable>(
      screen()->renderer()->device(),
      screen()->renderer()->quad_index_buffer());
  wave_quads_ = std::make_unique<renderer::QuadArray>(
      screen()->renderer()->device(),
      screen()->renderer()->quad_index_buffer());

  OnParentViewportRectChanged(parent_rect());
}

void Sprite::OnObjectDisposed() {
  RemoveFromList();

  wave_quads_.reset();
  drawable_quad_.reset();
}

void Sprite::PrepareDraw() {
  UpdateVisibilityInternal();
  if (need_invisible_)
    return;

  if (src_rect_need_update_) {
    src_rect_need_update_ = false;

    if (IsObjectValid(bitmap_.get())) {
      auto bitmap_size = bitmap_->GetSize();
      auto rect = src_rect_->AsBase();

      rect.width = std::clamp(rect.width, 0, bitmap_size.x - rect.x);
      rect.height = std::clamp(rect.height, 0, bitmap_size.y - rect.y);

      drawable_quad_->SetPosition(base::Vec2(static_cast<float>(rect.width),
                                             static_cast<float>(rect.height)));
      if (mirror_)
        drawable_quad_->SetTexcoord(
            base::Rect(rect.x + rect.width, rect.y, -rect.width, rect.height));
      else
        drawable_quad_->SetTexcoord(rect);
    }
  }

  if (wave_.need_update) {
    UpdateWaveQuadsInternal();
    wave_.need_update = false;
  }
}

void Sprite::OnDraw(CompositeTargetInfo* target_info) {
  if (need_invisible_)
    return;

  if (Flashable::IsFlashing() && Flashable::EmptyFlashing())
    return;

  const bool color_effect = color_->IsValid();
  const bool tone_effect = tone_->IsValid();
  const bool flash_effect = Flashable::IsFlashing();
  const bool bush_effect = bush_.depth != 0;
  const bool render_effect = color_effect || tone_effect || flash_effect ||
                             bush_effect || opacity_ != 255;

  auto bitmap_texture = bitmap_->GetHandle();
  auto bitmap_size = bitmap_->GetSize();
  base::Vec4 offset_texsize =
      base::MakeVec4(base::Vec2(), base::MakeInvert(bitmap_size));
  renderer::PipelineState* pso_state = nullptr;

  if (render_effect) {
    auto& shader = screen()->renderer()->GetPipelines()->sprite;
    pso_state = shader.GetPSOFor(blend_mode_);

    shader.SetTexture(
        bitmap_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

    auto invert_size = base::MakeInvert(bitmap_size);
    {
      Diligent::MapHelper<renderer::PipelineInstance_Sprite::VSUniform>
          Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, target_info->viewport_size,
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      memcpy(Constants->transformMat, transform_.GetMatrixDataUnsafe(),
             sizeof(float) * 16);
      Constants->texSize = invert_size;
    }

    {
      Diligent::MapHelper<renderer::PipelineInstance_Sprite::PSUniform>
          Constants(screen()->renderer()->context(), shader.GetPSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      Constants->texSize = invert_size;
      Constants->color = color_->AsBase();
      Constants->tone = tone_->AsBase();
      Constants->opacity = opacity_ / 255.0f;
      Constants->bushDepth = static_cast<float>(
          src_rect_->GetY() + src_rect_->GetHeight() - bush_.depth);
      Constants->bushOpacity = bush_.opacity / 255.0f;
    }
  } else {
    auto& shader = screen()->renderer()->GetPipelines()->basesprite;
    pso_state = shader.GetPSOFor(blend_mode_);

    shader.SetTexture(
        bitmap_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

    {
      Diligent::MapHelper<renderer::PipelineInstance_BaseSprite::VSUniform>
          Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, target_info->viewport_size,
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      memcpy(Constants->transformMat, transform_.GetMatrixDataUnsafe(),
             sizeof(float) * 16);
      Constants->texSize = base::MakeInvert(bitmap_size);
    }
  }

  screen()->renderer()->context()->SetPipelineState(pso_state->pso);
  screen()->renderer()->context()->CommitShaderResources(
      pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  if (wave_.active)
    wave_quads_->Draw(screen()->renderer()->context());
  else
    drawable_quad_->Draw(screen()->renderer()->context());
}

void Sprite::OnParentViewportRectChanged(
    const DrawableParent::ViewportInfo& viewport_rect) {
  transform_.SetGlobalOffset(viewport_rect.GetRealOffset());
}

void Sprite::OnSrcRectChangedInternal() {
  src_rect_need_update_ = true;
}

void Sprite::UpdateWaveQuadsInternal() {
  // Wave from other runtime
  // TODO: [deprecated] enhance wave process
  auto emitWaveChunk = [this](renderer::GeometryVertexLayout::Data*& vert,
                              float phase, int width, float zoomY, int chunkY,
                              int chunkLength) {
    float wavePos = phase + (chunkY / (float)wave_.length) * (float)(M_PI * 2);
    float chunkX = std::sin(wavePos) * wave_.amp;

    float chunkOffset = chunkY / zoomY;
    base::RectF tex(static_cast<float>(src_rect_->GetX()),
                    static_cast<float>(src_rect_->GetY() + chunkOffset),
                    static_cast<float>(width),
                    static_cast<float>(chunkLength / zoomY));
    base::RectF pos(base::Vec2(chunkX, chunkOffset), tex.Size());

    if (mirror_) {
      tex.x += tex.width;
      tex.width = -tex.width;
    }

    renderer::GeometryVertexLayout::SetPosition(vert, pos);
    renderer::GeometryVertexLayout::SetTexcoord(vert, tex);
    vert += 4;
  };

  if (!wave_.amp) {
    wave_.active = false;
    return;
  }

  wave_.active = true;

  int width = src_rect_->GetWidth();
  int height = src_rect_->GetHeight();
  float zoomY = transform_.GetScale().y;

  if (wave_.amp < -(width / 2)) {
    wave_quads_->Clear();
    return;
  }

  /* The length of the sprite as it appears on screen */
  int visibleLength = (int)(height * zoomY);

  /* First chunk length (aligned to 8 pixel boundary */
  int firstLength = ((int)transform_.GetPosition().y) % 8;

  /* Amount of full 8 pixel chunks in the middle */
  int chunks = (visibleLength - firstLength) / 8;

  /* Final chunk length */
  int lastLength = (visibleLength - firstLength) % 8;

  wave_quads_->Resize(screen()->renderer()->context(),
                      !!firstLength + chunks + !!lastLength);
  renderer::GeometryVertexLayout::Data* vert = wave_quads_->vertices().data();

  float phase = (wave_.phase * (float)M_PI) / 180.0f;

  if (firstLength > 0)
    emitWaveChunk(vert, phase, width, zoomY, 0, firstLength);

  for (int i = 0; i < chunks; ++i)
    emitWaveChunk(vert, phase, width, zoomY, firstLength + i * 8, 8);

  if (lastLength > 0)
    emitWaveChunk(vert, phase, width, zoomY, firstLength + chunks * 8,
                  lastLength);
}

void Sprite::UpdateVisibilityInternal() {
  need_invisible_ = true;

  if (!opacity_)
    return;

  if (!IsObjectValid(bitmap_.get()))
    return;

  if (wave_.active) {
    need_invisible_ = false;
    return;
  }

  const base::Vec2& scale = transform_.GetScale();
  if (scale.x != 1.0f || scale.y != 1.0f || transform_.GetRotation() != 0) {
    need_invisible_ = false;
    return;
  }

  auto& viewport = parent_rect();
  SDL_Rect screen_rect = viewport.rect.ToSDLRect();

  SDL_Rect self_rect;
  self_rect.w = bitmap_->GetSize().x;
  self_rect.h = bitmap_->GetSize().y;

  auto offset = viewport.GetRealOffset();
  self_rect.x =
      transform_.GetPosition().x - transform_.GetOrigin().x + offset.x;
  self_rect.y =
      transform_.GetPosition().y - transform_.GetOrigin().y + offset.y;

  need_invisible_ = !SDL_HasRectIntersection(&self_rect, &screen_rect);
}

}  // namespace content
