// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window2_impl.h"

#include "content/render/tilequad.h"
#include "renderer/utils/texture_utils.h"

namespace content {

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    ExceptionState& exception_state) {
  return base::MakeRefCounted<Window2Impl>(execution_context, nullptr,
                                           base::Rect(), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    ExceptionState& exception_state) {
  return base::MakeRefCounted<Window2Impl>(
      execution_context, ViewportImpl::From(viewport), base::Rect(), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    int32_t scale_,
                                    ExceptionState& exception_state) {
  return base::MakeRefCounted<Window2Impl>(
      execution_context, ViewportImpl::From(viewport), base::Rect(), scale_);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    ExceptionState& exception_state) {
  return base::MakeRefCounted<Window2Impl>(execution_context, nullptr,
                                           base::Rect(x, y, width, height), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t scale_,
                                    ExceptionState& exception_state) {
  return base::MakeRefCounted<Window2Impl>(
      execution_context, nullptr, base::Rect(x, y, width, height), scale_);
}

Window2Impl::Window2Impl(ExecutionContext* execution_context,
                         scoped_refptr<ViewportImpl> parent,
                         const base::Rect& bound,
                         int32_t scale_)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      rgss3_style_(execution_context->engine_profile->api_version ==
                   ContentProfile::APIVersion::RGSS3),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey(rgss3_style_ ? 100 : 0,
                    rgss3_style_ ? std::numeric_limits<int64_t>::max() : 0)),
      scale_(scale_),
      viewport_(parent),
      cursor_rect_(base::MakeRefCounted<RectImpl>(base::Rect())),
      bound_(bound),
      back_opacity_(rgss3_style_ ? 192 : 255),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &Window2Impl::DrawableNodeHandlerInternal, base::Unretained(this)));

  ExceptionState exception_state;
  contents_ =
      CanvasImpl::Create(execution_context, base::Vec2i(1, 1), exception_state);

  GPUCreateWindowInternal();
}

Window2Impl::~Window2Impl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void Window2Impl::SetLabel(const base::String& label,
                           ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

void Window2Impl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool Window2Impl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void Window2Impl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (!active_)
    return;

  pause_index_ = ++pause_index_ % 32;
  cursor_opacity_ += cursor_fade_ ? -8 : 8;
  if (cursor_opacity_ > 255) {
    cursor_opacity_ = 255;
    cursor_fade_ = true;
  } else if (cursor_opacity_ < 128) {
    cursor_opacity_ = 128;
    cursor_fade_ = false;
  }
}

void Window2Impl::Move(int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height,
                       ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_ = base::Rect(x, y, width, height);
}

bool Window2Impl::IsOpened(ExceptionState& exception_state) {
  return openness_ == 255;
}

bool Window2Impl::IsClosed(ExceptionState& exception_state) {
  return openness_ == 0;
}

scoped_refptr<Viewport> Window2Impl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void Window2Impl::Put_Viewport(const scoped_refptr<Viewport>& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

scoped_refptr<Bitmap> Window2Impl::Get_Windowskin(
    ExceptionState& exception_state) {
  return windowskin_;
}

void Window2Impl::Put_Windowskin(const scoped_refptr<Bitmap>& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  windowskin_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Bitmap> Window2Impl::Get_Contents(
    ExceptionState& exception_state) {
  return contents_;
}

void Window2Impl::Put_Contents(const scoped_refptr<Bitmap>& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Rect> Window2Impl::Get_CursorRect(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return cursor_rect_;
}

void Window2Impl::Put_CursorRect(const scoped_refptr<Rect>& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *cursor_rect_ = *RectImpl::From(value);
}

bool Window2Impl::Get_Active(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return active_;
}

void Window2Impl::Put_Active(const bool& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  active_ = value;
}

bool Window2Impl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return node_.GetVisibility();
}

void Window2Impl::Put_Visible(const bool& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeVisibility(value);
}

bool Window2Impl::Get_ArrowsVisible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return arrows_visible_;
}

void Window2Impl::Put_ArrowsVisible(const bool& value,
                                    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  arrows_visible_ = value;
}

bool Window2Impl::Get_Pause(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return pause_;
}

void Window2Impl::Put_Pause(const bool& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  pause_ = value;
}

int32_t Window2Impl::Get_X(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.x;
}

void Window2Impl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.x = value;
}

int32_t Window2Impl::Get_Y(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.y;
}

void Window2Impl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.y = value;
}

int32_t Window2Impl::Get_Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.width;
}

void Window2Impl::Put_Width(const int32_t& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.width = value;
}

int32_t Window2Impl::Get_Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.height;
}

void Window2Impl::Put_Height(const int32_t& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.height = value;
}

int32_t Window2Impl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return node_.GetSortKeys()->weight[0];
}

void Window2Impl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeSortWeight(value);
}

int32_t Window2Impl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.x;
}

void Window2Impl::Put_Ox(const int32_t& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.x = value;
}

int32_t Window2Impl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.y;
}

void Window2Impl::Put_Oy(const int32_t& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.y = value;
}

int32_t Window2Impl::Get_Padding(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return padding_;
}

void Window2Impl::Put_Padding(const int32_t& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  padding_ = value;
  padding_bottom_ = padding_;
}

int32_t Window2Impl::Get_PaddingBottom(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return padding_bottom_;
}

void Window2Impl::Put_PaddingBottom(const int32_t& value,
                                    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  padding_bottom_ = value;
}

int32_t Window2Impl::Get_Opacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return opacity_;
}

void Window2Impl::Put_Opacity(const int32_t& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_BackOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return back_opacity_;
}

void Window2Impl::Put_BackOpacity(const int32_t& value,
                                  ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  back_opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_ContentsOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return contents_opacity_;
}

void Window2Impl::Put_ContentsOpacity(const int32_t& value,
                                      ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_Openness(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return openness_;
}

void Window2Impl::Put_Openness(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  openness_ = std::clamp(value, 0, 255);
}

scoped_refptr<Tone> Window2Impl::Get_Tone(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return tone_;
}

void Window2Impl::Put_Tone(const scoped_refptr<Tone>& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *tone_ = *ToneImpl::From(value);
}

void Window2Impl::OnObjectDisposed() {
  node_.DisposeNode();

  Agent empty_agent;
  std::swap(agent_, empty_agent);
}

void Window2Impl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  BitmapAgent* windowskin_agent =
      windowskin_ ? windowskin_->GetAgent() : nullptr;
  BitmapAgent* contents_agent = contents_ ? contents_->GetAgent() : nullptr;

  base::Rect padding_rect =
      base::Rect(padding_, padding_, std::max(0, bound_.width - padding_ * 2),
                 std::max(0, bound_.height - (padding_ + padding_bottom_)));

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeWindowQuadsInternal(params->context, contents_agent,
                                    windowskin_agent, padding_rect);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderWindowQuadsInternal(params->context, params->world_binding);
  }
}

void Window2Impl::GPUCreateWindowInternal() {
  agent_.batch = renderer::QuadBatch::Make(**context()->render_device);

  auto* pipelines = context()->render_device->GetPipelines();
  agent_.base_binding = pipelines->base.CreateBinding();
  agent_.flat_binding = pipelines->viewport.CreateBinding();
  agent_.content_binding = pipelines->base.CreateBinding();
  agent_.display_binding = pipelines->base.CreateBinding();

  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::Binding_Flat::Params),
      "window2.uniform", &agent_.uniform, Diligent::USAGE_DEFAULT);
}

void Window2Impl::GPUCompositeWindowQuadsInternal(
    renderer::RenderContext* render_context,
    BitmapAgent* contents,
    BitmapAgent* windowskin,
    const base::Rect& padding_rect) {
  // Reset texture if size changed
  if (!agent_.texture ||
      static_cast<int32_t>(agent_.texture->GetDesc().Width) != bound_.width ||
      static_cast<int32_t>(agent_.texture->GetDesc().Height) != bound_.height) {
    agent_.texture.Release();
    renderer::CreateTexture2D(
        **context()->render_device, &agent_.texture, "window2.texture",
        bound_.Size(), Diligent::USAGE_DEFAULT,
        Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);

    renderer::WorldTransform world_matrix;
    renderer::MakeProjectionMatrix(world_matrix.projection, bound_.Size());
    renderer::MakeIdentityMatrix(world_matrix.transform);

    agent_.world.Release();
    Diligent::CreateUniformBuffer(
        **context()->render_device, sizeof(world_matrix), "window2.world",
        &agent_.world, Diligent::USAGE_IMMUTABLE, Diligent::BIND_UNIFORM_BUFFER,
        Diligent::CPU_ACCESS_NONE, &world_matrix);
  }

  // Prepare quad count
  int32_t quad_count = 1;

  if (windowskin) {
    // Background 1
    quad_count += 1;
    // Background 2
    const int32_t horizon_tiles =
        CalculateQuadTileCount(32 * scale_, bound_.width - 2 * scale_);
    const int32_t vertical_tiles =
        CalculateQuadTileCount(32 * scale_, bound_.height - 2 * scale_);
    quad_count += horizon_tiles * vertical_tiles;

    // Corners
    quad_count += 4;
    // Frames
    const int32_t horizon_frame_tiles =
        CalculateQuadTileCount(16 * scale_, bound_.width - 16 * scale_);
    const int32_t vertical_frame_tiles =
        CalculateQuadTileCount(16 * scale_, bound_.height - 16 * scale_);
    quad_count += horizon_frame_tiles * 2 + vertical_frame_tiles * 2;

    // Arrows
    quad_count += 4;

    // Pause anime
    quad_count += 1;

    // Cursor
    quad_count += 9;
  }

  if (contents) {
    // Contents (1)
    quad_count += 1;
  }

  // Generate quads
  auto& quads = agent_.cache;
  quads.resize(quad_count);
  int32_t quad_index = 0;

  // Draw attributes
  const float opacity_norm = opacity_ / 255.0f;
  const float openness_norm = openness_ / 255.0f;
  const float back_opacity_norm = back_opacity_ / 255.0f;
  const float contents_opacity_norm = contents_opacity_ / 255.0f;
  const float cursor_opacity_norm = cursor_opacity_ / 255.0f;

  int32_t tiled_quads_count = 0;
  int32_t cursor_quads_offset = 0;
  int32_t frames_quads_count = 0;
  int32_t contents_quads_offset = 0;
  int32_t cursor_draw_quads = 0;
  int32_t arrows_pause_quads = 0;

  if (windowskin) {
    const base::Rect background1_src(0, 0, 32 * scale_, 32 * scale_);
    const base::Rect background2_src(0, 32 * scale_, 32 * scale_, 32 * scale_);
    const base::Rect background_dest(scale_, scale_, bound_.width - 2 * scale_,
                                     bound_.height - 2 * scale_);

    // Stretch layer (1)
    renderer::Quad::SetPositionRect(&quads[quad_index], background_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], background1_src,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index],
                             base::Vec4(back_opacity_norm * opacity_norm));
    quad_index++;

    // Tiled layer (NxM)
    tiled_quads_count = BuildTiles(background2_src, background_dest,
                                   base::Vec4(back_opacity_norm * opacity_norm),
                                   windowskin->size, &quads[quad_index]);
    quad_index += tiled_quads_count;

    // Corners (4)
    const base::Rect corner_left_top_src(32 * scale_, 0, 8 * scale_,
                                         8 * scale_);
    const base::Rect corner_right_top_src(56 * scale_, 0, 8 * scale_,
                                          8 * scale_);
    const base::Rect corner_left_bottom_src(32 * scale_, 24 * scale_,
                                            8 * scale_, 8 * scale_);
    const base::Rect corner_right_bottom_src(56 * scale_, 24 * scale_,
                                             8 * scale_, 8 * scale_);

    const base::Rect corner_left_top_dest(0, 0, 8 * scale_, 8 * scale_);
    const base::Rect corner_right_top_dest(bound_.width - 8 * scale_, 0,
                                           8 * scale_, 8 * scale_);
    const base::Rect corner_left_bottom_dest(0, bound_.height - 8 * scale_,
                                             8 * scale_, 8 * scale_);
    const base::Rect corner_right_bottom_dest(bound_.width - 8 * scale_,
                                              bound_.height - 8 * scale_,
                                              8 * scale_, 8 * scale_);

    renderer::Quad::SetPositionRect(&quads[quad_index], corner_left_top_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_top_src,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index], corner_right_top_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_top_src,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    corner_left_bottom_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_bottom_src,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    corner_right_bottom_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_bottom_src,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    frames_quads_count += 4;

    // Tiles frame (Nx2 + Mx2)
    const base::Rect frame_up_src(40 * scale_, 0, 16 * scale_, 8 * scale_);
    const base::Rect frame_down_src(40 * scale_, 24 * scale_, 16 * scale_,
                                    8 * scale_);
    const base::Rect frame_left_src(32 * scale_, 8 * scale_, 8 * scale_,
                                    16 * scale_);
    const base::Rect frame_right_src(56 * scale_, 8 * scale_, 8 * scale_,
                                     16 * scale_);

    int32_t tiled_frames_quads = 0;
    tiled_frames_quads = BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, frame_up_src, base::Vec2i(8 * scale_, 0),
        base::Vec4(opacity_norm), bound_.width - 16 * scale_, windowskin->size,
        &quads[quad_index]);
    quad_index += tiled_frames_quads;
    frames_quads_count += tiled_frames_quads;
    tiled_frames_quads = BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, frame_down_src,
        base::Vec2i(8 * scale_, bound_.height - 8 * scale_),
        base::Vec4(opacity_norm), bound_.width - 16 * scale_, windowskin->size,
        &quads[quad_index]);
    quad_index += tiled_frames_quads;
    frames_quads_count += tiled_frames_quads;
    tiled_frames_quads = BuildTilesAlongAxis(
        TileAxis::VERTICAL, frame_left_src, base::Vec2i(0, 8 * scale_),
        base::Vec4(opacity_norm), bound_.height - 16 * scale_, windowskin->size,
        &quads[quad_index]);
    quad_index += tiled_frames_quads;
    frames_quads_count += tiled_frames_quads;
    tiled_frames_quads = BuildTilesAlongAxis(
        TileAxis::VERTICAL, frame_right_src,
        base::Vec2i(bound_.width - 8 * scale_, 8 * scale_),
        base::Vec4(opacity_norm), bound_.height - 16 * scale_, windowskin->size,
        &quads[quad_index]);
    quad_index += tiled_frames_quads;
    frames_quads_count += tiled_frames_quads;
  }

  // Controls & Contents
  if (openness_ >= 255) {
    // Padding offset
    base::Vec2i content_offset = padding_rect.Position();

    // Controls
    if (windowskin) {
      const base::Vec2i arrow_display_offset =
          (bound_.Size() - base::Vec2i(8 * scale_)) / 2;

      // Arrows (4)
      if (arrows_visible_) {
        const base::Rect arrow_up_dest(arrow_display_offset.x, 2 * scale_,
                                       8 * scale_, 4 * scale_);
        const base::Rect arrow_down_dest(arrow_display_offset.x,
                                         bound_.height - 6 * scale_, 8 * scale_,
                                         4 * scale_);
        const base::Rect arrow_left_dest(2 * scale_, arrow_display_offset.y,
                                         4 * scale_, 8 * scale_);
        const base::Rect arrow_right_dest(bound_.width - 6 * scale_,
                                          arrow_display_offset.y, 4 * scale_,
                                          8 * scale_);

        const base::Rect arrow_up_src(44 * scale_, 8 * scale_, 8 * scale_,
                                      4 * scale_);
        const base::Rect arrow_down_src(44 * scale_, 20 * scale_, 8 * scale_,
                                        4 * scale_);
        const base::Rect arrow_left_src(40 * scale_, 12 * scale_, 4 * scale_,
                                        8 * scale_);
        const base::Rect arrow_right_src(52 * scale_, 12 * scale_, 4 * scale_,
                                         8 * scale_);

        if (contents) {
          if (origin_.x > 0) {
            renderer::Quad::SetPositionRect(&quads[quad_index],
                                            arrow_left_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_left_src,
                                            windowskin->size);
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            arrows_pause_quads++;
          }
          if (origin_.y > 0) {
            renderer::Quad::SetPositionRect(&quads[quad_index], arrow_up_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_up_src,
                                            windowskin->size);
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            arrows_pause_quads++;
          }
          if (padding_rect.width < (contents->size.x - origin_.x)) {
            renderer::Quad::SetPositionRect(&quads[quad_index],
                                            arrow_right_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_right_src,
                                            windowskin->size);
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            arrows_pause_quads++;
          }
          if (padding_rect.height < (contents->size.y - origin_.y)) {
            renderer::Quad::SetPositionRect(&quads[quad_index],
                                            arrow_down_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_down_src,
                                            windowskin->size);
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            arrows_pause_quads++;
          }
        }
      }

      // Pause (1)
      const base::Rect pause_src[] = {
          {48 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
          {56 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
          {48 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
          {56 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
      };

      if (pause_) {
        const base::Rect pause_dest(arrow_display_offset.x,
                                    bound_.height - 8 * scale_, 8 * scale_,
                                    8 * scale_);
        renderer::Quad::SetPositionRect(&quads[quad_index], pause_dest);
        renderer::Quad::SetTexCoordRect(
            &quads[quad_index], pause_src[pause_index_ / 8], windowskin->size);
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
        arrows_pause_quads++;
      }

      // Cursor (9)
      const auto cursor_rect = cursor_rect_->AsBaseRect();
      if (cursor_rect.width > 0 && cursor_rect.height > 0) {
        auto build_cursor_internal = [&](const base::Rect& rect,
                                         base::Rect quad_rects[9]) {
          int32_t w = rect.width;
          int32_t h = rect.height;
          int32_t x1 = rect.x;
          int32_t x2 = x1 + w;
          int32_t y1 = rect.y;
          int32_t y2 = y1 + h;

          int32_t i = 0;
          // Left-Top
          quad_rects[i++] = base::Rect(x1, y1, scale_, scale_);
          // Right-Top
          quad_rects[i++] = base::Rect(x2 - scale_, y1, scale_, scale_);
          // Right-Bottom
          quad_rects[i++] =
              base::Rect(x2 - scale_, y2 - scale_, scale_, scale_);
          // Left-Bottom
          quad_rects[i++] = base::Rect(x1, y2 - scale_, scale_, scale_);
          // Left
          quad_rects[i++] = base::Rect(x1, y1 + scale_, scale_, h - scale_ * 2);
          // Right
          quad_rects[i++] =
              base::Rect(x2 - scale_, y1 + scale_, scale_, h - scale_ * 2);
          // Top
          quad_rects[i++] = base::Rect(x1 + scale_, y1, w - scale_ * 2, scale_);
          // Bottom
          quad_rects[i++] =
              base::Rect(x1 + scale_, y2 - scale_, w - scale_ * 2, scale_);
          // Center
          quad_rects[i++] = base::Rect(x1 + scale_, y1 + scale_, w - scale_ * 2,
                                       h - scale_ * 2);
        };

        auto build_cursor_quads = [&](const base::Rect& src,
                                      const base::Rect& dst,
                                      renderer::Quad vert[9]) {
          base::Rect quad_rects[9];

          build_cursor_internal(src, quad_rects);
          for (int32_t i = 0; i < 9; ++i)
            renderer::Quad::SetTexCoordRect(&vert[i], quad_rects[i],
                                            windowskin->size);

          build_cursor_internal(dst, quad_rects);
          for (int32_t i = 0; i < 9; ++i)
            renderer::Quad::SetPositionRect(&vert[i], quad_rects[i]);

          const base::Vec4 color(cursor_opacity_norm * contents_opacity_norm);
          for (int32_t i = 0; i < 9; ++i)
            renderer::Quad::SetColor(&vert[i], color);

          return 9;
        };

        const base::Rect cursor_src(32 * scale_, 32 * scale_, 16 * scale_,
                                    16 * scale_);
        if (cursor_rect.width > 0 && cursor_rect.height > 0) {
          base::Rect cursor_dest(content_offset + cursor_rect.Position(),
                                 cursor_rect.Size());

          if (rgss3_style_) {
            cursor_dest.x -= origin_.x;
            cursor_dest.y -= origin_.y;
          }

          cursor_draw_quads =
              build_cursor_quads(cursor_src, cursor_dest, &quads[quad_index]);
          cursor_quads_offset = quad_index;
          quad_index += cursor_draw_quads;
        }
      }
    }

    // Contents (1)
    if (contents) {
      renderer::Quad::SetPositionRect(
          &quads[quad_index],
          base::Rect(content_offset - origin_, contents->size));
      renderer::Quad::SetTexCoordRectNorm(&quads[quad_index], base::Rect(0, 1));
      renderer::Quad::SetColor(&quads[quad_index],
                               base::Vec4(contents_opacity_norm));
      contents_quads_offset = quad_index;
      quad_index++;
    }
  }

  // Screen display quad (1)
  const base::Rect background_dest(
      bound_.x,
      static_cast<float>(bound_.y) +
          static_cast<float>(bound_.height / 2.0f) * (1.0f - openness_norm),
      bound_.width, bound_.height * openness_norm);
  renderer::Quad::SetPositionRect(&quads[quad_index], background_dest);
  renderer::Quad::SetTexCoordRectNorm(&quads[quad_index], base::Rect(0, 1));
  agent_.display_quad_offset = quad_index;
  quad_index++;

  // Update GPU vertex buffer
  agent_.batch.QueueWrite(**render_context, quads.data(), quads.size());

  // Make sure index buffer count
  context()->render_device->GetQuadIndex()->Allocate(quad_index);

  // Update background tone uniform buffer
  renderer::Binding_Flat::Params uniform;
  uniform.Color = base::Vec4();
  uniform.Tone = tone_->AsNormColor();
  (*render_context)
      ->UpdateBuffer(agent_.uniform, 0, sizeof(uniform), &uniform,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Prepare render pipelines
  auto& pipeline_set_tone = context()->render_device->GetPipelines()->viewport;
  auto& pipeline_set_base = context()->render_device->GetPipelines()->base;

  auto* pipeline_tone =
      pipeline_set_tone.GetPipeline(renderer::BLEND_TYPE_NORMAL, false);
  auto* pipeline_tone_alpha =
      pipeline_set_tone.GetPipeline(renderer::BLEND_TYPE_KEEP_ALPHA, false);
  auto* pipeline_base =
      pipeline_set_base.GetPipeline(renderer::BLEND_TYPE_NORMAL, false);

  // Setup render pass
  auto* render_target_view =
      agent_.texture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  (*render_context)
      ->SetRenderTargets(1, &render_target_view, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->ClearRenderTarget(render_target_view, nullptr,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup shader binding
  if (windowskin) {
    agent_.base_binding.u_transform->Set(agent_.world);
    agent_.base_binding.u_texture->Set(windowskin->resource);

    agent_.flat_binding.u_transform->Set(agent_.world);
    agent_.flat_binding.u_texture->Set(windowskin->resource);
    agent_.flat_binding.u_params->Set(agent_.uniform);
  }

  if (contents) {
    agent_.content_binding.u_transform->Set(agent_.world);
    agent_.content_binding.u_texture->Set(contents->resource);
  }

  // Apply global vertex and index
  Diligent::IBuffer* const vertex_buffer = *agent_.batch;
  (*render_context)
      ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Reset scissor test
  render_context->ScissorState()->Apply(bound_.Size());

  // Execute texture composite
  if (windowskin) {
    int32_t quads_draw_offset = 0;

    // Stretch layer pass
    (*render_context)->SetPipelineState(pipeline_tone);
    (*render_context)
        ->CommitShaderResources(
            *agent_.flat_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
      quads_draw_offset++;
    }

    // Tiled layer pass
    (*render_context)->SetPipelineState(pipeline_tone_alpha);
    (*render_context)
        ->CommitShaderResources(
            *agent_.flat_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = tiled_quads_count * 6;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = quads_draw_offset * 6;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
      quads_draw_offset += tiled_quads_count;
    }

    // Corners & Frames
    (*render_context)->SetPipelineState(pipeline_base);
    (*render_context)
        ->CommitShaderResources(
            *agent_.base_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = frames_quads_count * 6;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = quads_draw_offset * 6;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
      quads_draw_offset += frames_quads_count;
    }

    if (openness_ >= 255) {
      // Arrows & Pause
      (*render_context)->SetPipelineState(pipeline_base);
      (*render_context)
          ->CommitShaderResources(
              *agent_.base_binding,
              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      {
        Diligent::DrawIndexedAttribs draw_indexed_attribs;
        draw_indexed_attribs.NumIndices = arrows_pause_quads * 6;
        draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
        draw_indexed_attribs.FirstIndexLocation = quads_draw_offset * 6;
        (*render_context)->DrawIndexed(draw_indexed_attribs);
        quads_draw_offset += arrows_pause_quads;
      }

      // Clamped cursor region
      if (cursor_draw_quads) {
        render_context->ScissorState()->Push(padding_rect);

        // Cursor rect
        if (cursor_quads_offset) {
          (*render_context)->SetPipelineState(pipeline_base);
          (*render_context)
              ->CommitShaderResources(
                  *agent_.base_binding,
                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

          {
            Diligent::DrawIndexedAttribs draw_indexed_attribs;
            draw_indexed_attribs.NumIndices = cursor_draw_quads * 6;
            draw_indexed_attribs.IndexType =
                renderer::QuadIndexCache::kValueType;
            draw_indexed_attribs.FirstIndexLocation = cursor_quads_offset * 6;
            (*render_context)->DrawIndexed(draw_indexed_attribs);
          }
        }

        render_context->ScissorState()->Pop();
      }
    }
  }

  if (contents && openness_ >= 255) {
    render_context->ScissorState()->Push(padding_rect);

    // Contents pass
    (*render_context)->SetPipelineState(pipeline_base);
    (*render_context)
        ->CommitShaderResources(
            *agent_.content_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = contents_quads_offset * 6;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
    }

    render_context->ScissorState()->Pop();
  }
}

void Window2Impl::GPURenderWindowQuadsInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding) {
  auto& pipeline_set = context()->render_device->GetPipelines()->base;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

  // Setup world uniform
  agent_.display_binding.u_transform->Set(world_binding);
  agent_.display_binding.u_texture->Set(
      agent_.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.batch;
  (*render_context)
      ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply pipeline state
  (*render_context)->SetPipelineState(pipeline);
  (*render_context)
      ->CommitShaderResources(
          *agent_.display_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  draw_indexed_attribs.FirstIndexLocation = agent_.display_quad_offset * 6;
  (*render_context)->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content
