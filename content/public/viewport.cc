// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/viewport.h"

#include "content/public/bitmap.h"
#include "content/public/utility.h"
#include "renderer/device/render_device.h"

namespace content {

Viewport::Viewport(scoped_refptr<Graphics> screen)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen, nullptr) {
  InitViewportInternal(screen->GetSize());
}

Viewport::Viewport(scoped_refptr<Graphics> screen, const base::Rect& rect)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen, nullptr) {
  InitViewportInternal(rect);
}

Viewport::Viewport(scoped_refptr<Graphics> screen,
                   scoped_refptr<Viewport> viewport)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen, viewport) {
  InitViewportInternal(viewport->GetRect()->AsBase().Size());
}

Viewport::~Viewport() {
  Dispose();
}

void Viewport::SetOX(int ox) {
  CheckIsDisposed();

  if (viewport_rect().origin.x == ox)
    return;

  viewport_rect().origin.x = ox;
  NotifyViewportRectChanged();
}

void Viewport::SetOY(int oy) {
  CheckIsDisposed();

  if (viewport_rect().origin.y == oy)
    return;

  viewport_rect().origin.y = oy;
  NotifyViewportRectChanged();
}

void Viewport::SetRect(scoped_refptr<Rect> rect) {
  CheckIsDisposed();

  if (rect->IsSame(*rect_))
    return;

  *rect_ = *rect;
  OnRectChangedInternal();
}

void Viewport::SnapToBitmap(scoped_refptr<Bitmap> target) {
  CheckIsDisposed();

  DrawableParent::PrepareComposite();

  auto bitmap_handle = target->GetHandle();
  auto* RTV =
      bitmap_handle->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  float ClearColor[4] = {0, 0, 0, 1};
  screen()->renderer()->context()->ClearRenderTarget(
      RTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  CompositeTargetInfo target_info;
  target_info.render_target = RTV;
  target_info.viewport_size = target->GetSize();
  target_info.scissor_region = target_info.viewport_size;

  Diligent::Rect scissor;
  scissor.right = target->GetSize().x;
  scissor.bottom = target->GetSize().y;
  screen()->renderer()->context()->SetScissorRects(
      1, &scissor, 1, scissor.bottom + scissor.left);

  DrawableParent::Composite(&target_info);

  screen()->renderer()->context()->Flush();
}

void Viewport::SetViewport(scoped_refptr<Viewport> viewport) {
  ViewportChild::SetViewport(viewport);

  OnRectChangedInternal();
}

void Viewport::OnObjectDisposed() {
  RemoveFromList();
}

void Viewport::PrepareDraw() {
  DrawableParent::PrepareComposite();
}

void Viewport::OnDraw(CompositeTargetInfo* target_info) {
  if (Flashable::IsFlashing() && Flashable::EmptyFlashing())
    return;

  // Push and set scissor stack
  base::Rect scissor_state_cache;
  std::swap(target_info->scissor_region, scissor_state_cache);
  target_info->scissor_region =
      base::MakeIntersect(viewport_rect().rect, scissor_state_cache);

  {
    Diligent::Rect scissor;
    scissor.left = target_info->scissor_region.x;
    scissor.right = scissor.left + target_info->scissor_region.width;
    scissor.top = target_info->scissor_region.y;
    scissor.bottom = scissor.top + target_info->scissor_region.height;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);
  }

  // Execute children draw command
  DrawableParent::Composite(target_info);

  if (Flashable::IsFlashing() || color_->IsValid() || tone_->IsValid()) {
    base::Vec4 composite_color = color_->AsBase();
    base::Vec4 flash_color = Flashable::GetFlashColor();
    base::Vec4 target_color;
    if (Flashable::IsFlashing())
      target_color =
          (flash_color.w > composite_color.w ? flash_color : composite_color);
    else
      target_color = composite_color;

    // Composite viewport effect
    ApplyViewportEffect(target_info->render_target, viewport_rect().rect,
                        target_color, tone_->AsBase());
  }

  // Resume parent scissor setting
  {
    Diligent::Rect scissor;
    scissor.left = scissor_state_cache.x;
    scissor.right = scissor.left + scissor_state_cache.width;
    scissor.top = scissor_state_cache.y;
    scissor.bottom = scissor.top + scissor_state_cache.height;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);
  }

  // Pop scissor stack
  std::swap(target_info->scissor_region, scissor_state_cache);
}

void Viewport::OnParentViewportRectChanged(const ViewportInfo& rect) {
  OnRectChangedInternal();
}

void Viewport::InitViewportInternal(const base::Rect& initial_rect) {
  rect_ = new Rect(initial_rect);
  rect_observer_ = rect_->AddChangedObserver(base::BindRepeating(
      &Viewport::OnRectChangedInternal, base::Unretained(this)));

  color_ = new Color();
  tone_ = new Tone();

  OnRectChangedInternal();
}

void Viewport::OnRectChangedInternal() {
  viewport_rect().rect = rect_->AsBase();

  base::Vec2i offset = parent_rect().GetRealOffset();
  viewport_rect().rect.x += offset.x;
  viewport_rect().rect.y += offset.y;

  NotifyViewportRectChanged();
}

void Viewport::ApplyViewportEffect(Diligent::ITextureView* target_buffer,
                                   const base::Rect& blend_area,
                                   const base::Vec4& color,
                                   const base::Vec4& tone) {
  const bool has_tone_effect =
      (tone.x != 0 || tone.y != 0 || tone.z != 0 || tone.w != 0);
  const bool has_color_effect = color.w != 0;

  if (!has_tone_effect && !has_color_effect)
    return;

  auto intermediate_cache = screen()->renderer()->MakeGenericFramebuffer(
      blend_area.Size(), screen()->tex_format());

  screen()->renderer()->context()->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto target_size = base::Vec2i(target_buffer->GetTexture()->GetDesc().Width,
                                 target_buffer->GetTexture()->GetDesc().Height);
  Diligent::Box SrcBox(blend_area.x, blend_area.x + blend_area.width,
                       blend_area.y, blend_area.y + blend_area.height);
  renderer::ClampBox(&SrcBox, target_size);
  Diligent::CopyTextureAttribs CopyTexAttr(
      target_buffer->GetTexture(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, intermediate_cache,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  CopyTexAttr.pSrcBox = &SrcBox;
  screen()->renderer()->context()->CopyTexture(CopyTexAttr);

  screen()->renderer()->context()->SetRenderTargets(
      1, &target_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& shader = screen()->renderer()->GetPipelines()->viewport;
  auto* pso = shader.GetPSOFor(renderer::BlendType::NoBlend);
  screen()->renderer()->context()->SetPipelineState(pso->pso);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Viewport::VSUniform>
        Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                  Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_size,
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize =
        base::MakeInvert(base::Vec2i(intermediate_cache->GetDesc().Width,
                                     intermediate_cache->GetDesc().Height));
    Constants->transOffset = base::Vec2i(0);
  }

  {
    Diligent::MapHelper<renderer::PipelineInstance_Viewport::PSUniform>
        Constants(screen()->renderer()->context(), shader.GetPSUniform(),
                  Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    Constants->color = color;
    Constants->tone = tone;
  }

  shader.SetTexture(intermediate_cache->GetDefaultView(
      Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  screen()->renderer()->context()->CommitShaderResources(
      pso->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto* quad = screen()->renderer()->common_quad();
  quad->SetPosition(blend_area);
  quad->SetTexcoord(base::Vec2(blend_area.Size()));
  quad->Draw(screen()->renderer()->context());
}

ViewportChild::ViewportChild(scoped_refptr<Graphics> screen,
                             scoped_refptr<Viewport> viewport,
                             int z,
                             int sprite_y)
    : Drawable(viewport ? static_cast<DrawableParent*>(viewport.get())
                        : screen.get(),
               z,
               true,
               sprite_y),
      screen_(screen) {}

void ViewportChild::SetViewport(scoped_refptr<Viewport> viewport) {
  CheckObjectDisposed();

  if (viewport == viewport_)
    return;
  viewport_ = viewport;

  DrawableParent* parent = viewport_.get();
  if (!parent)
    parent = screen_.get();

  SetParent(parent);
  OnParentViewportRectChanged(parent->viewport_rect());
}

}  // namespace content