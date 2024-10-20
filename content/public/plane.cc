// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/plane.h"

#include <algorithm>

namespace {

float fwrap(float value, float range) {
  float res = std::fmod(value, range);
  return res < 0 ? res + range : res;
}

}  // namespace

namespace content {

Plane::Plane(scoped_refptr<Graphics> screen, scoped_refptr<Viewport> viewport)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen, viewport),
      color_(new Color()),
      tone_(new Tone()) {
  quad_array_ = std::make_unique<renderer::QuadArray>(
      screen->renderer()->device(), screen->renderer()->quad_index_buffer());

  OnParentViewportRectChanged(parent_rect());
}

Plane::~Plane() {
  Dispose();
}

void Plane::SetBitmap(scoped_refptr<Bitmap> bitmap) {
  CheckIsDisposed();

  if (bitmap_ == bitmap)
    return;
  bitmap_ = bitmap;

  if (IsObjectValid(bitmap_.get()))
    quad_array_dirty_ = true;
}

void Plane::SetOX(int ox) {
  CheckIsDisposed();

  if (ox_ == ox)
    return;

  ox_ = ox;
  quad_array_dirty_ = true;
}

void Plane::SetOY(int oy) {
  CheckIsDisposed();

  if (oy_ == oy)
    return;

  oy_ = oy;
  quad_array_dirty_ = true;
}

void Plane::SetZoomX(double zoom_x) {
  CheckIsDisposed();

  if (zoom_x_ == zoom_x)
    return;

  zoom_x_ = zoom_x;
  quad_array_dirty_ = true;
}

void Plane::SetZoomY(double zoom_y) {
  CheckIsDisposed();

  if (zoom_y_ == zoom_y)
    return;

  zoom_y_ = zoom_y;
  quad_array_dirty_ = true;
}

void Plane::OnObjectDisposed() {
  RemoveFromList();

  quad_array_.reset();
  cache_layer_.Release();
}

void Plane::PrepareDraw() {
  if (quad_array_dirty_) {
    UpdateQuadArray();
    quad_array_dirty_ = false;
  }
}

void Plane::OnDraw(CompositeTargetInfo* target_info) {
  if (!opacity_)
    return;
  if (!IsObjectValid(bitmap_.get()))
    return;

  auto bitmap_texture = bitmap_->GetHandle();
  auto bitmap_size = bitmap_->GetSize();
  renderer::PipelineState* pso_state = nullptr;

  if (color_->IsValid() || tone_->IsValid() || opacity_ != 255) {
    auto& shader = screen()->renderer()->GetPipelines()->viewport;
    pso_state = shader.GetPSOFor(blend_type_);

    shader.SetTexture(
        bitmap_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

    {
      Diligent::MapHelper<renderer::PipelineInstance_Viewport::VSUniform>
          Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, target_info->viewport_size,
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      Constants->transOffset = parent_rect().GetRealOffset();
      Constants->texSize = base::MakeInvert(bitmap_size);
    }

    {
      Diligent::MapHelper<renderer::PipelineInstance_Viewport::PSUniform>
          Constants(screen()->renderer()->context(), shader.GetPSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      Constants->color = color_->AsBase();
      Constants->tone = tone_->AsBase();
      Constants->opacity = opacity_ / 255.0f;
    }
  } else {
    auto& shader = screen()->renderer()->GetPipelines()->base;
    pso_state = shader.GetPSOFor(blend_type_);

    shader.SetTexture(
        bitmap_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

    {
      Diligent::MapHelper<renderer::PipelineInstance_Base::VSUniform> Constants(
          screen()->renderer()->context(), shader.GetVSUniform(),
          Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, target_info->viewport_size,
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      Constants->transOffset = parent_rect().GetRealOffset();
      Constants->texSize = base::MakeInvert(bitmap_size);
    }
  }

  screen()->renderer()->context()->SetPipelineState(pso_state->pso);
  screen()->renderer()->context()->CommitShaderResources(
      pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  quad_array_->Draw(screen()->renderer()->context());
}

void Plane::OnParentViewportRectChanged(const DrawableParent::ViewportInfo&) {
  quad_array_dirty_ = true;
}

void Plane::UpdateQuadArray() {
  if (!IsObjectValid(bitmap_.get()))
    return;

  auto bitmap_size = bitmap_->GetSize();

  const float item_x =
      std::max(1.0f, static_cast<float>(bitmap_size.x * zoom_x_));
  const float item_y =
      std::max(1.0f, static_cast<float>(bitmap_size.y * zoom_y_));

  const float wrap_ox = fwrap(ox_, item_x);
  const float wrap_oy = fwrap(oy_, item_y);

  const int tile_x =
      std::ceil((parent_rect().rect.width + wrap_ox - item_x) / item_x) + 1;
  const int tile_y =
      std::ceil((parent_rect().rect.height + wrap_oy - item_y) / item_y) + 1;

  quad_array_->Resize(screen()->renderer()->context(), tile_x * tile_y);

  for (int y = 0; y < tile_y; ++y) {
    for (int x = 0; x < tile_x; ++x) {
      size_t index = (y * tile_x + x) * 4;
      renderer::GeometryVertexLayout::Data* vert =
          &quad_array_->vertices()[index];
      base::RectF pos(x * item_x - wrap_ox, y * item_y - wrap_oy, item_x,
                      item_y);

      renderer::GeometryVertexLayout::SetPosition(vert, pos);
      renderer::GeometryVertexLayout::SetTexcoord(vert,
                                                  base::Vec2(item_x, item_y));
    }
  }
}

}  // namespace content
