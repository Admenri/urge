// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/window2.h"

#include "content/common/tilequad.h"
#include "content/common/tileutils.h"

namespace content {

/* Asset rect fix data from Other runtime source */
static const base::Rect background_stretch_src = {0, 0, 64, 64};
static const base::Rect frame_tile_src = {64, 0, 64, 64};
static const base::Rect background_tile_src = {0, 64, 64, 64};

static const Corners<base::Rect> corner_src = {
    base::Rect(64, 0, 16, 16), base::Rect(112, 0, 16, 16),
    base::Rect(64, 48, 16, 16), base::Rect(112, 48, 16, 16)};

static const Sides<base::Rect> border_src = {
    base::Rect(64, 16, 16, 32), base::Rect(112, 16, 16, 32),
    base::Rect(80, 0, 32, 16), base::Rect(80, 48, 32, 16)};

static const Sides<base::Rect> scroll_arrow_src = {
    base::Rect(80, 24, 8, 16), base::Rect(104, 24, 8, 16),
    base::Rect(88, 16, 16, 8), base::Rect(88, 40, 16, 8)};

static const base::Rect pause_src[4] = {
    base::Rect(96, 64, 16, 16), base::Rect(112, 64, 16, 16),
    base::Rect(96, 80, 16, 16), base::Rect(112, 80, 16, 16)};

static const uint8_t pause_quad[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};

static const uint8_t pause_alpha[] = {0x00, 0x20, 0x40, 0x60, 0x80,
                                      0xA0, 0xC0, 0xE0, 0xFF};

struct CursorSrc {
  Corners<base::Rect> corners;
  Sides<base::Rect> border;
  base::Rect background;
};

static const CursorSrc cursor_src = {
    {base::Rect(64, 64, 4, 4), base::Rect(92, 64, 4, 4),
     base::Rect(64, 92, 4, 4), base::Rect(92, 92, 4, 4)},
    {base::Rect(64, 68, 4, 24), base::Rect(92, 68, 4, 24),
     base::Rect(68, 64, 24, 4), base::Rect(68, 92, 24, 4)},
    base::Rect(68, 68, 24, 24)};

static const uint8_t cursor_alpha[] = {
    /* Fade out */
    0xFF, 0xF7, 0xEF, 0xE7, 0xDF, 0xD7, 0xCF, 0xC7, 0xBF, 0xB7, 0xAF, 0xA7,
    0x9F, 0x97, 0x8F, 0x87, 0x7F, 0x77, 0x6F, 0x67,
    /* Fade in */
    0x5F, 0x67, 0x6F, 0x77, 0x7F, 0x87, 0x8F, 0x97, 0x9F, 0xA7, 0xAF, 0xB7,
    0xBF, 0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7};

static const uint8_t cursor_alpha_reset = 0x10;

Window2::Window2(scoped_refptr<Graphics> screen,
                 scoped_refptr<Viewport> viewport)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen,
                    viewport,
                    (screen->api_version() >= APIVersion::RGSS3 ? 100 : 0),
                    (screen->api_version() >= APIVersion::RGSS3
                         ? std::numeric_limits<int>::max()
                         : 0)),
      rect_(0, 0, 0, 0) {
  InitWindow();
}

Window2::Window2(scoped_refptr<Graphics> screen,
                 int x,
                 int y,
                 int width,
                 int height)
    : GraphicsElement(screen.get()),
      Disposable(screen.get()),
      ViewportChild(screen,
                    nullptr,
                    (screen->api_version() >= APIVersion::RGSS3 ? 100 : 0),
                    (screen->api_version() >= APIVersion::RGSS3
                         ? std::numeric_limits<int>::max()
                         : 0)),
      rect_(x, y, width, height) {
  InitWindow();
}

Window2::~Window2() {
  Dispose();
}

void Window2::Update() {
  CheckIsDisposed();

  update_required_ = true;
}

void Window2::Move(int x, int y, int width, int height) {
  CheckIsDisposed();

  width = std::max(0, width);
  height = std::max(0, height);

  if (rect_ == base::Rect(x, y, width, height))
    return;

  rect_ = base::Rect(x, y, width, height);
  base_tex_need_update_ = true;
  base_quad_need_update_ = true;
  arrows_quad_need_update_ = true;
}

void Window2::SetWindowskin(scoped_refptr<Bitmap> windowskin) {
  CheckIsDisposed();

  if (windowskin_ == windowskin)
    return;

  windowskin_ = windowskin;
  base_tex_need_update_ = true;

  if (IsObjectValid(windowskin_.get()))
    windowskin_observer_ = windowskin_->AddBitmapObserver(base::BindRepeating(
        &Window2::WindowskinChangedInternal, base::Unretained(this)));
}

void Window2::SetContents(scoped_refptr<Bitmap> contents) {
  CheckIsDisposed();

  if (contents_ == contents)
    return;

  contents_ = contents;
  arrows_quad_need_update_ = true;
  contents_quad_need_update_ = true;
}

void Window2::SetCursorRect(scoped_refptr<Rect> cursor_rect) {
  CheckIsDisposed();

  if (cursor_rect_->IsSame(*cursor_rect))
    return;

  *cursor_rect_ = *cursor_rect;
  cursor_quad_need_update_ = true;
}

void Window2::SetActive(bool active) {
  CheckIsDisposed();

  if (active_ == active)
    return;

  active_ = active;
  cursor_alpha_index_ = cursor_alpha_reset;
  cursor_step_need_update_ = true;
}

void Window2::SetArrowsVisible(bool arrows_visible) {
  CheckIsDisposed();

  if (arrows_visible_ == arrows_visible)
    return;

  arrows_visible_ = arrows_visible;
  arrows_quad_need_update_ = true;
}

void Window2::SetPause(bool pause) {
  CheckIsDisposed();

  if (pause_ == pause)
    return;

  pause_ = pause;
  pause_alpha_index_ = 0;
  pause_quad_index_ = 0;
  arrows_quad_need_update_ = true;
}

void Window2::SetX(int x) {
  CheckIsDisposed();
  rect_.x = x;
}

void Window2::SetY(int y) {
  CheckIsDisposed();
  rect_.y = y;
}

void Window2::SetWidth(int width) {
  CheckIsDisposed();

  width = std::max(0, width);

  if (rect_.width == width)
    return;

  rect_.width = width;
  base_tex_need_update_ = true;
  base_quad_need_update_ = true;
  arrows_quad_need_update_ = true;
}

void Window2::SetHeight(int height) {
  CheckIsDisposed();

  height = std::max(0, height);

  if (rect_.height == height)
    return;

  rect_.height = height;
  base_tex_need_update_ = true;
  base_quad_need_update_ = true;
  arrows_quad_need_update_ = true;
}

void Window2::SetOX(int ox) {
  CheckIsDisposed();

  if (ox_ == ox)
    return;

  ox_ = ox;
  arrows_quad_need_update_ = true;
}

void Window2::SetOY(int oy) {
  CheckIsDisposed();

  if (oy_ == oy)
    return;

  oy_ = oy;
  arrows_quad_need_update_ = true;
}

void Window2::SetPadding(int padding) {
  CheckIsDisposed();

  if (padding_ == padding)
    return;

  padding_ = padding;
  padding_bottom_ = padding;
}

void Window2::SetPaddingBottom(int padding_bottom) {
  CheckIsDisposed();

  if (padding_bottom_ == padding_bottom)
    return;

  padding_bottom_ = padding_bottom;
}

void Window2::SetOpacity(int opacity) {
  CheckIsDisposed();

  opacity = std::clamp(opacity, 0, 255);

  if (opacity_ == opacity)
    return;

  opacity_ = opacity;
  base_quad_need_update_ = true;
}

void Window2::SetBackOpacity(int back_opacity) {
  CheckIsDisposed();

  back_opacity = std::clamp(back_opacity, 0, 255);

  if (back_opacity_ == back_opacity)
    return;

  back_opacity_ = back_opacity;
  base_tex_need_update_ = true;
}

void Window2::SetContentsOpacity(int contents_opacity) {
  CheckIsDisposed();

  contents_opacity = std::clamp(contents_opacity, 0, 255);

  if (contents_opacity_ == contents_opacity)
    return;

  contents_opacity_ = contents_opacity;
  contents_quad_need_update_ = true;
}

void Window2::SetOpenness(int openness) {
  CheckIsDisposed();

  openness = std::clamp(openness, 0, 255);

  if (openness_ == openness)
    return;

  openness_ = openness;
  base_quad_need_update_ = true;
}

void Window2::SetTone(scoped_refptr<Tone> tone) {
  CheckIsDisposed();

  if (tone_->IsSame(*tone))
    return;

  *tone_ = *tone;
  base_tex_need_update_ = true;
}

void Window2::OnObjectDisposed() {
  RemoveFromList();

  renderer_data_.reset();
  base_texture_.Release();
}

void Window2::PrepareDraw() {
  if (update_required_) {
    update_required_ = false;
    UpdateInternal();
  }

  padding_rect_ =
      base::Rect(padding_, padding_, std::max(0, rect_.width - padding_ * 2),
                 std::max(0, rect_.height - (padding_ + padding_bottom_)));

  if (base_tex_need_update_) {
    base_tex_need_update_ = false;
    UpdateBaseTextureInternal();
  }

  if (base_quad_need_update_) {
    base_quad_need_update_ = false;
    UpdateBaseQuadInternal();
  }

  if (arrows_quad_need_update_) {
    arrows_quad_need_update_ = false;
    CalcArrowsQuadArrayInternal();
    UpdatePauseStepInternal();
  }

  if (cursor_quad_need_update_) {
    cursor_quad_need_update_ = false;
    UpdateCursorQuadsInternal();
    cursor_step_need_update_ = true;
  }

  if (cursor_step_need_update_) {
    cursor_step_need_update_ = false;
    if (renderer_data_->cursor_quads->count() > 0) {
      base::Vec4 color(0, 0, 0, cursor_alpha[cursor_alpha_index_] / 255.0f);
      for (size_t i = 0; i < renderer_data_->cursor_quads->count(); ++i)
        renderer::GeometryVertexLayout::SetColor(
            &renderer_data_->cursor_quads->vertices()[i * 4], color);
    }
  }

  if (contents_quad_need_update_) {
    contents_quad_need_update_ = false;
    if (IsObjectValid(contents_.get())) {
      base::Rect contents_rect = contents_->GetSize();
      renderer_data_->content_quad->SetTexcoord(contents_rect);
      renderer_data_->content_quad->SetPosition(contents_rect);
      renderer_data_->content_quad->SetColor(
          base::Vec4(0, 0, 0, static_cast<float>(contents_opacity_) / 255.0f));
    }
  }
}

void Window2::OnDraw(CompositeTargetInfo* target_info) {
  bool windowskin_valid = IsObjectValid(windowskin_.get());
  bool contents_valid = IsObjectValid(contents_.get());

  base::Vec2i trans_offset = rect_.Position() + parent_rect().GetRealOffset();

  // Push scissor
  {
    auto intersect = base::MakeIntersect(base::Rect(trans_offset, rect_.Size()),
                                         target_info->scissor_region);

    Diligent::Rect scissor;
    scissor.left = intersect.x;
    scissor.right = scissor.left + intersect.width;
    scissor.top = intersect.y;
    scissor.bottom = scissor.top + intersect.height;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);
  }

  /* Stretch background & frame */
  auto& shader = screen()->renderer()->GetPipelines()->basealpha;
  auto* pso_state = shader.GetPSOFor(renderer::BlendType::Normal);
  screen()->renderer()->context()->SetPipelineState(pso_state->pso);

  float proj_mat[16];
  renderer::MakeProjectionMatrix(
      proj_mat, target_info->viewport_size,
      screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());

  if (windowskin_valid) {
    if (opacity_ > 0) {
      {
        Diligent::MapHelper<renderer::PipelineInstance_BaseAlpha::VSUniform>
            Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                      Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        memcpy(Constants->projMat, proj_mat, sizeof(proj_mat));
        Constants->transOffset = trans_offset;
        Constants->texSize = base::MakeInvert(rect_.Size());
      }

      shader.SetTexture(base_texture_->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      /* Base window draw */
      renderer_data_->base_quad->Draw(screen()->renderer()->context());
    }

    if (openness_ == 255 && arrows_quad_count_) {
      {
        Diligent::MapHelper<renderer::PipelineInstance_BaseAlpha::VSUniform>
            Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                      Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        memcpy(Constants->projMat, proj_mat, sizeof(proj_mat));
        Constants->transOffset = trans_offset;
        Constants->texSize = base::MakeInvert(windowskin_->GetSize());
      }

      shader.SetTexture(windowskin_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      /* Controls draw */
      renderer_data_->arrows_quads->Draw(screen()->renderer()->context(), 0,
                                         arrows_quad_count_);
    }
  }

  if (openness_ == 255) {
    base::Rect padding_trans_rect = padding_rect_;
    padding_trans_rect.x += trans_offset.x;
    padding_trans_rect.y += trans_offset.y;

    if (screen()->api_version() >= APIVersion::RGSS3) {
      auto intersect =
          base::MakeIntersect(padding_trans_rect, target_info->scissor_region);

      Diligent::Rect scissor;
      scissor.left = intersect.x;
      scissor.right = scissor.left + intersect.width;
      scissor.top = intersect.y;
      scissor.bottom = scissor.top + intersect.height;
      screen()->renderer()->context()->SetScissorRects(
          1, &scissor, 1, scissor.bottom + scissor.left);
    }

    /* Cursor draw */
    if (renderer_data_->cursor_quads->count() > 0 && windowskin_valid) {
      {
        Diligent::MapHelper<renderer::PipelineInstance_BaseAlpha::VSUniform>
            Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                      Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        memcpy(Constants->projMat, proj_mat, sizeof(proj_mat));
        Constants->transOffset = trans_offset;
        Constants->texSize = base::MakeInvert(windowskin_->GetSize());
      }

      shader.SetTexture(windowskin_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      base::Vec2i cursor_trans = padding_trans_rect.Position();
      cursor_trans.x += cursor_rect_->GetX();
      cursor_trans.y += cursor_rect_->GetY();

      if (screen()->api_version() >= APIVersion::RGSS3)
        cursor_trans = cursor_trans - base::Vec2i(ox_, oy_);

      {
        Diligent::MapHelper<renderer::PipelineInstance_BaseAlpha::VSUniform>
            Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                      Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        memcpy(Constants->projMat, proj_mat, sizeof(proj_mat));
        Constants->transOffset = cursor_trans;
        Constants->texSize = base::MakeInvert(windowskin_->GetSize());
      }

      renderer_data_->cursor_quads->Draw(screen()->renderer()->context());
    }

    /* Window contents */
    if (contents_valid && contents_opacity_ > 0) {
      if (screen()->api_version() < APIVersion::RGSS3) {
        auto intersect = base::MakeIntersect(padding_trans_rect,
                                             target_info->scissor_region);

        Diligent::Rect scissor;
        scissor.left = intersect.x;
        scissor.right = scissor.left + intersect.width;
        scissor.top = intersect.y;
        scissor.bottom = scissor.top + intersect.height;
        screen()->renderer()->context()->SetScissorRects(
            1, &scissor, 1, scissor.bottom + scissor.left);
      }

      base::Vec2i content_trans = padding_trans_rect.Position();
      content_trans = content_trans - base::Vec2i(ox_, oy_);

      {
        Diligent::MapHelper<renderer::PipelineInstance_BaseAlpha::VSUniform>
            Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                      Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
        memcpy(Constants->projMat, proj_mat, sizeof(proj_mat));
        Constants->transOffset = content_trans;
        Constants->texSize = base::MakeInvert(contents_->GetSize());
      }

      shader.SetTexture(contents_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      renderer_data_->content_quad->Draw(screen()->renderer()->context());
    }
  }

  // Pop scissor
  {
    Diligent::Rect scissor;
    scissor.left = target_info->scissor_region.x;
    scissor.right = scissor.left + target_info->scissor_region.width;
    scissor.top = target_info->scissor_region.y;
    scissor.bottom = scissor.top + target_info->scissor_region.height;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);
  }
}

void Window2::CheckObjectDisposed() const {
  CheckIsDisposed();
}

void Window2::OnParentViewportRectChanged(
    const DrawableParent::ViewportInfo& rect) {
  // Passed on composite trans calculate
}

void Window2::InitWindow() {
  padding_ = screen()->api_version() >= APIVersion::RGSS3 ? 12 : 16;
  back_opacity_ = screen()->api_version() >= APIVersion::RGSS3 ? 192 : 255;

  tone_ = new Tone();
  tone_observer_ = tone_->AddChangedObserver(base::BindRepeating(
      &Window2::ToneChangedInternal, base::Unretained(this)));

  contents_ = new Bitmap(static_cast<Graphics*>(screen()), base::Vec2i(1, 1));

  cursor_rect_ = new Rect();
  cursor_rect_observer_ = cursor_rect_->AddChangedObserver(base::BindRepeating(
      &Window2::CursorRectChangedInternal, base::Unretained(this)));

  renderer_data_.reset(new Window2RendererData);
  auto dec = screen()->renderer()->device();
  auto indices = screen()->renderer()->quad_index_buffer();

  renderer_data_->base_quad =
      std::make_unique<renderer::QuadDrawable>(dec, indices);
  renderer_data_->content_quad =
      std::make_unique<renderer::QuadDrawable>(dec, indices);
  renderer_data_->arrows_quads =
      std::make_unique<renderer::QuadArray>(dec, indices);
  renderer_data_->cursor_quads =
      std::make_unique<renderer::QuadArray>(dec, indices);
  renderer_data_->base_tex_quad_array =
      std::make_unique<renderer::QuadArray>(dec, indices);

  pause_vertex_ = nullptr;
  contents_quad_need_update_ = true;

  /*
   * Stretch layer
   * Frame layer
   * Cursor layer
   * Control layer
   * Contents layer
   */
}

void Window2::CalcBaseQuadArrayInternal() {
  size_t quad_count = 0;

  /* Base layer (background layer & frame layer) */
  /* Stretch layer x 1 */
  quad_count += 1;

  /* Tiled layer x 2 */
  /* Background layer */
  const base::Rect background_rect{2, 2, rect_.width - 4, rect_.height - 4};
  base_bg_tile_count_ =
      QuadTileCount2D(frame_tile_src.width, frame_tile_src.height,
                      background_rect.width, background_rect.height);
  quad_count += base_bg_tile_count_;

  /* Frame layer */
  base_frame_tile_count_ = 4;
  /* Frame sides */
  const base::Vec2i frame_size{rect_.width - 16 * 2, rect_.height - 16 * 2};
  if (frame_size.x > 0) {
    base_frame_tile_count_ += QuadTileCount(32, frame_size.x) * 2;
  }
  if (frame_size.y > 0) {
    base_frame_tile_count_ += QuadTileCount(32, frame_size.y) * 2;
  }
  quad_count += base_frame_tile_count_;

  /* Set vertex data */
  renderer_data_->base_tex_quad_array->Resize(screen()->renderer()->context(),
                                              quad_count);

  /* Fill vertex data */
  renderer::GeometryVertexLayout::Data* vertex =
      renderer_data_->base_tex_quad_array->vertices().data();

  int i = 0;
  /* Stretch background */
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vertex[i * 4], background_stretch_src, background_rect);

  /* Tiled background */
  i += BuildTiles<renderer::GeometryVertexLayout::Data>(
      &vertex[i * 4], background_tile_src, background_rect);

  /* Frame Corners */
  const Corners<base::Rect> corner_pos = {
      base::Rect(0, 0, 16, 16),                               /* Top left */
      base::Rect(rect_.width - 16, 0, 16, 16),                /* Top right */
      base::Rect(0, rect_.height - 16, 16, 16),               /* Bottom left */
      base::Rect(rect_.width - 16, rect_.height - 16, 16, 16) /* Bottom right */
  };

  i += renderer::GeometryVertexLayout::SetTexPos(
      &vertex[i * 4], corner_src.top_left, corner_pos.top_left);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vertex[i * 4], corner_src.top_right, corner_pos.top_right);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vertex[i * 4], corner_src.bottom_left, corner_pos.bottom_left);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vertex[i * 4], corner_src.bottom_right, corner_pos.bottom_right);

  /* Frame tile sides */
  if (frame_size.x > 0) {
    i += BuildTileH<renderer::GeometryVertexLayout::Data>(
        &vertex[i * 4], border_src.top, frame_size.x, 16, 0);
    i += BuildTileH<renderer::GeometryVertexLayout::Data>(
        &vertex[i * 4], border_src.bottom, frame_size.x, 16, rect_.height - 16);
  }

  if (frame_size.y > 0) {
    i += BuildTileV<renderer::GeometryVertexLayout::Data>(
        &vertex[i * 4], border_src.left, frame_size.y, 0, 16);
    i += BuildTileV<renderer::GeometryVertexLayout::Data>(
        &vertex[i * 4], border_src.right, frame_size.y, rect_.width - 16, 16);
  }
}

void Window2::UpdateBaseTextureInternal() {
  if (rect_.width <= 0 || rect_.height <= 0)
    return;

  CalcBaseQuadArrayInternal();

  {
    Diligent::TextureDesc TexDesc;
    TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    TexDesc.Format = screen()->tex_format();
    TexDesc.Usage = Diligent::USAGE_DEFAULT;
    TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
    TexDesc.BindFlags =
        Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    TexDesc.Width = rect_.width;
    TexDesc.Height = rect_.height;

    base_texture_.Release();
    screen()->renderer()->device()->CreateTexture(TexDesc, nullptr,
                                                  &base_texture_);
  }

  auto* RTV =
      base_texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  float ClearColor[4] = {0, 0, 0, 0};
  screen()->renderer()->context()->ClearRenderTarget(
      RTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  if (!IsObjectValid(windowskin_.get()))
    return;

  {
    Diligent::Rect scissor;
    scissor.right = rect_.width;
    scissor.bottom = rect_.height;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);
  }

  {
    auto& shader = screen()->renderer()->GetPipelines()->viewport;

    {
      Diligent::MapHelper<renderer::PipelineInstance_Viewport::VSUniform>
          Constants(screen()->renderer()->context(), shader.GetVSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, rect_.Size(),
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      Constants->texSize = base::MakeInvert(windowskin_->GetSize());
      Constants->transOffset = base::Vec2i(0);
    }

    {
      Diligent::MapHelper<renderer::PipelineInstance_Viewport::PSUniform>
          Constants(screen()->renderer()->context(), shader.GetPSUniform(),
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      Constants->tone = tone_->AsBase();
      Constants->color = base::Vec4(0);
      Constants->opacity = back_opacity_ / 255.0f;
    }

    {
      auto* pso_state = shader.GetPSOFor(renderer::BlendType::NoBlend);
      screen()->renderer()->context()->SetPipelineState(pso_state->pso);

      shader.SetTexture(windowskin_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      /* Draw stretch layer */
      renderer_data_->base_tex_quad_array->Draw(screen()->renderer()->context(),
                                                0, 1);
    }

    {
      auto* pso_state = shader.GetPSOFor(renderer::BlendType::KeepDestAlpha);
      screen()->renderer()->context()->SetPipelineState(pso_state->pso);

      shader.SetTexture(windowskin_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      /* Background tiles */
      renderer_data_->base_tex_quad_array->Draw(screen()->renderer()->context(),
                                                1, base_bg_tile_count_);
    }
  }

  {
    auto& shader = screen()->renderer()->GetPipelines()->base;

    {
      Diligent::MapHelper<renderer::PipelineInstance_Base::VSUniform> Constants(
          screen()->renderer()->context(), shader.GetVSUniform(),
          Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, rect_.Size(),
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      Constants->texSize = base::MakeInvert(windowskin_->GetSize());
      Constants->transOffset = base::Vec2i(0);
    }

    {
      auto* pso_state = shader.GetPSOFor(renderer::BlendType::Normal);
      screen()->renderer()->context()->SetPipelineState(pso_state->pso);

      shader.SetTexture(windowskin_->GetHandle()->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
      screen()->renderer()->context()->CommitShaderResources(
          pso_state->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      /* Frame corner and sides */
      renderer_data_->base_tex_quad_array->Draw(screen()->renderer()->context(),
                                                1 + base_bg_tile_count_,
                                                base_frame_tile_count_);
    }
  }
}

void Window2::UpdateBaseQuadInternal() {
  const float openness = openness_ / 255.0f;
  const base::Rect tex(0, 0, rect_.width, rect_.height);
  const base::Rect pos(0, (rect_.height / 2.0f) * (1.0f - openness),
                       rect_.width, rect_.height * openness);

  renderer_data_->base_quad->SetTexcoord(tex);
  renderer_data_->base_quad->SetPosition(pos);
  renderer_data_->base_quad->SetColor(base::Vec4(0, 0, 0, opacity_ / 255.0f));
}

void Window2::CalcArrowsQuadArrayInternal() {
  /* Scroll arrow position: Top Bottom X, Left Right Y */
  const base::Vec2i arrow_size =
      (rect_.Size() - base::Vec2i(16, 16)) / base::Vec2i(2, 2);

  const Sides<base::Rect> arrowPos = {
      base::Rect(4, arrow_size.y, 8, 16),                /* Left */
      base::Rect(rect_.width - 12, arrow_size.y, 8, 16), /* Right */
      base::Rect(arrow_size.x, 4, 16, 8),                /* Top */
      base::Rect(arrow_size.x, rect_.height - 12, 16, 8) /* Bottom */
  };

  size_t i = 0;
  renderer_data_->arrows_quads->Resize(screen()->renderer()->context(), 6);
  renderer::GeometryVertexLayout::Data* vert =
      renderer_data_->arrows_quads->vertices().data();

  if (IsObjectValid(contents_.get()) && arrows_visible_) {
    if (ox_ > 0)
      i += renderer::GeometryVertexLayout::SetTexPos(
          &vert[i * 4], scroll_arrow_src.left, arrowPos.left);
    if (oy_ > 0)
      i += renderer::GeometryVertexLayout::SetTexPos(
          &vert[i * 4], scroll_arrow_src.top, arrowPos.top);

    if (padding_rect_.width < (contents_->GetSize().x - ox_))
      i += renderer::GeometryVertexLayout::SetTexPos(
          &vert[i * 4], scroll_arrow_src.right, arrowPos.right);
    if (padding_rect_.height < (contents_->GetSize().y - oy_))
      i += renderer::GeometryVertexLayout::SetTexPos(
          &vert[i * 4], scroll_arrow_src.bottom, arrowPos.bottom);
  }

  pause_vertex_ = nullptr;
  if (pause_) {
    pause_vertex_ = &vert[i * 4];
    const base::Rect pausePos(arrow_size.x, rect_.height - 16, 16, 16);
    i += renderer::GeometryVertexLayout::SetTexPos(pause_vertex_, pause_src[0],
                                                   pausePos);
  }

  arrows_quad_count_ = i;
}

void Window2::UpdateInternal() {
  if (active_)
    if (++cursor_alpha_index_ ==
        (sizeof(cursor_alpha) / sizeof(cursor_alpha[0])))
      cursor_alpha_index_ = 0;

  if (pause_) {
    if (pause_alpha_index_ <
        static_cast<int>(sizeof(pause_alpha) / sizeof(pause_alpha[0]) - 1))
      ++pause_alpha_index_;

    if (++pause_quad_index_ ==
        static_cast<int>(sizeof(pause_quad) / sizeof(pause_quad[0])))
      pause_quad_index_ = 0;
  }

  UpdatePauseStepInternal();
  cursor_step_need_update_ = true;
}

void Window2::ToneChangedInternal() {
  base_tex_need_update_ = true;
}

void Window2::CursorRectChangedInternal() {
  cursor_quad_need_update_ = true;
}

void Window2::WindowskinChangedInternal() {
  base_tex_need_update_ = true;
}

void Window2::UpdatePauseStepInternal() {
  if (pause_vertex_) {
    renderer::GeometryVertexLayout::SetTexcoord(
        pause_vertex_, pause_src[pause_quad[pause_quad_index_]]);
    renderer::GeometryVertexLayout::SetColor(
        pause_vertex_,
        base::Vec4(0, 0, 0, pause_alpha[pause_alpha_index_] / 255.0f));
  }
}

void Window2::UpdateCursorQuadsInternal() {
  const base::Rect rect = cursor_rect_->AsBase();
  const CursorSrc& src = cursor_src;

  if (rect.width == 0 || rect.height == 0) {
    renderer_data_->cursor_quads->Clear();
    return;
  }

  const base::Vec2 corOff(rect.width - 4, rect.height - 4);

  const Corners<base::Rect> cornerPos = {
      base::Rect(0, 0, 4, 4),              /* Top left */
      base::Rect(corOff.x, 0, 4, 4),       /* Top right */
      base::Rect(0, corOff.y, 4, 4),       /* Bottom left */
      base::Rect(corOff.x, corOff.y, 4, 4) /* Bottom right */
  };

  const base::Vec2i sideLen(rect.width - 4 * 2, rect.height - 4 * 2);

  const Sides<base::Rect> sidePos = {
      base::Rect(0, 4, 4, sideLen.y),        /* Left */
      base::Rect(corOff.x, 4, 4, sideLen.y), /* Right */
      base::Rect(4, 0, sideLen.x, 4),        /* Top */
      base::Rect(4, corOff.y, sideLen.x, 4)  /* Bottom */
  };

  const base::Rect bgPos(4, 4, sideLen.x, sideLen.y);

  bool drawSidesLR = rect.height > 8;
  bool drawSidesTB = rect.width > 8;
  bool drawBg = drawSidesLR && drawSidesTB;

  size_t quads = 0;
  quads += 4; /* 4 corners */

  if (drawSidesLR)
    quads += 2;

  if (drawSidesTB)
    quads += 2;

  if (drawBg)
    quads += 1;

  renderer_data_->cursor_quads->Resize(screen()->renderer()->context(), quads);
  renderer::GeometryVertexLayout::Data* vert =
      renderer_data_->cursor_quads->vertices().data();
  size_t i = 0;

  i += renderer::GeometryVertexLayout::SetTexPos(
      &vert[i * 4], src.corners.top_left, cornerPos.top_left);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vert[i * 4], src.corners.top_right, cornerPos.top_right);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vert[i * 4], src.corners.bottom_left, cornerPos.bottom_left);
  i += renderer::GeometryVertexLayout::SetTexPos(
      &vert[i * 4], src.corners.bottom_right, cornerPos.bottom_right);

  if (drawSidesLR) {
    i += renderer::GeometryVertexLayout::SetTexPos(
        &vert[i * 4], src.border.left, sidePos.left);
    i += renderer::GeometryVertexLayout::SetTexPos(
        &vert[i * 4], src.border.right, sidePos.right);
  }

  if (drawSidesTB) {
    i += renderer::GeometryVertexLayout::SetTexPos(&vert[i * 4], src.border.top,
                                                   sidePos.top);
    i += renderer::GeometryVertexLayout::SetTexPos(
        &vert[i * 4], src.border.bottom, sidePos.bottom);
  }

  if (drawBg)
    renderer::GeometryVertexLayout::SetTexPos(&vert[i * 4], src.background,
                                              bgPos);
}

}  // namespace content
