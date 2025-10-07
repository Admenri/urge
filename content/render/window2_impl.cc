// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window2_impl.h"

#include "content/context/execution_context.h"
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
      rgss3_style_(execution_context->engine_profile->api_version >=
                   ContentProfile::APIVersion::RGSS3),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey(rgss3_style_ ? 100 : 0,
                    rgss3_style_ ? std::numeric_limits<int64_t>::max() : 0)),
      scale_(scale_),
      viewport_(parent),
      cursor_rect_(base::MakeRefCounted<RectImpl>(base::Rect())),
      bound_(bound),
      padding_(rgss3_style_ ? 12 : 16),
      padding_bottom_(padding_),
      back_opacity_(rgss3_style_ ? 192 : 255),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &Window2Impl::DrawableNodeHandlerInternal, base::Unretained(this)));

  ExceptionState exception_state;
  contents_ =
      CanvasImpl::Create(execution_context, base::Vec2i(1, 1), exception_state);

  GPUCreateWindowInternal();
}

DISPOSABLE_DEFINITION(Window2Impl);

void Window2Impl::SetLabel(const std::string& label,
                           ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

void Window2Impl::Update(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (pause_)
    pause_index_ = (++pause_index_) % 32;

  if (active_) {
    cursor_opacity_ += cursor_fade_ ? -8 : 8;
    if (cursor_opacity_ > 255) {
      cursor_opacity_ = 255;
      cursor_fade_ = true;
    } else if (cursor_opacity_ < 128) {
      cursor_opacity_ = 128;
      cursor_fade_ = false;
    }
  }
}

void Window2Impl::Move(int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height,
                       ExceptionState& exception_state) {
  DISPOSE_CHECK;

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
  DISPOSE_CHECK;

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
  DISPOSE_CHECK;

  windowskin_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Bitmap> Window2Impl::Get_Contents(
    ExceptionState& exception_state) {
  return contents_;
}

void Window2Impl::Put_Contents(const scoped_refptr<Bitmap>& value,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK;

  contents_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Rect> Window2Impl::Get_CursorRect(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return cursor_rect_;
}

void Window2Impl::Put_CursorRect(const scoped_refptr<Rect>& value,
                                 ExceptionState& exception_state) {
  DISPOSE_CHECK;

  CHECK_ATTRIBUTE_VALUE;

  *cursor_rect_ = *RectImpl::From(value);
}

bool Window2Impl::Get_Active(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return active_;
}

void Window2Impl::Put_Active(const bool& value,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  active_ = value;
}

bool Window2Impl::Get_Visible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return node_.GetVisibility();
}

void Window2Impl::Put_Visible(const bool& value,
                              ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeVisibility(value);
}

bool Window2Impl::Get_ArrowsVisible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return arrows_visible_;
}

void Window2Impl::Put_ArrowsVisible(const bool& value,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  arrows_visible_ = value;
}

bool Window2Impl::Get_Pause(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return pause_;
}

void Window2Impl::Put_Pause(const bool& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  pause_ = value;
}

int32_t Window2Impl::Get_X(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return bound_.x;
}

void Window2Impl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  bound_.x = value;
}

int32_t Window2Impl::Get_Y(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return bound_.y;
}

void Window2Impl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  bound_.y = value;
}

int32_t Window2Impl::Get_Width(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return bound_.width;
}

void Window2Impl::Put_Width(const int32_t& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  bound_.width = value;
}

int32_t Window2Impl::Get_Height(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return bound_.height;
}

void Window2Impl::Put_Height(const int32_t& value,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  bound_.height = value;
}

int32_t Window2Impl::Get_Z(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return node_.GetSortKeys()->weight[0];
}

void Window2Impl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeSortWeight(value);
}

int32_t Window2Impl::Get_Ox(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return origin_.x;
}

void Window2Impl::Put_Ox(const int32_t& value,
                         ExceptionState& exception_state) {
  DISPOSE_CHECK;

  origin_.x = value;
}

int32_t Window2Impl::Get_Oy(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return origin_.y;
}

void Window2Impl::Put_Oy(const int32_t& value,
                         ExceptionState& exception_state) {
  DISPOSE_CHECK;

  origin_.y = value;
}

int32_t Window2Impl::Get_Padding(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return padding_;
}

void Window2Impl::Put_Padding(const int32_t& value,
                              ExceptionState& exception_state) {
  DISPOSE_CHECK;

  padding_ = value;
  padding_bottom_ = padding_;
}

int32_t Window2Impl::Get_PaddingBottom(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return padding_bottom_;
}

void Window2Impl::Put_PaddingBottom(const int32_t& value,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  padding_bottom_ = value;
}

int32_t Window2Impl::Get_Opacity(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return opacity_;
}

void Window2Impl::Put_Opacity(const int32_t& value,
                              ExceptionState& exception_state) {
  DISPOSE_CHECK;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_BackOpacity(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return back_opacity_;
}

void Window2Impl::Put_BackOpacity(const int32_t& value,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK;

  back_opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_ContentsOpacity(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return contents_opacity_;
}

void Window2Impl::Put_ContentsOpacity(const int32_t& value,
                                      ExceptionState& exception_state) {
  DISPOSE_CHECK;

  contents_opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_Openness(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return openness_;
}

void Window2Impl::Put_Openness(const int32_t& value,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK;

  openness_ = std::clamp(value, 0, 255);
}

scoped_refptr<Tone> Window2Impl::Get_Tone(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return tone_;
}

void Window2Impl::Put_Tone(const scoped_refptr<Tone>& value,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

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

  GPUBitmapData* windowskin_agent = Disposable::IsValid(windowskin_.get())
                                        ? windowskin_->GetGPUData()
                                        : nullptr;
  GPUBitmapData* contents_agent =
      Disposable::IsValid(contents_.get()) ? contents_->GetGPUData() : nullptr;

  base::Rect padding_rect =
      base::Rect(padding_, padding_, std::max(0, bound_.width - padding_ * 2),
                 std::max(0, bound_.height - (padding_ + padding_bottom_)));

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeWindowQuadsInternal(params->context, contents_agent,
                                    windowskin_agent, padding_rect);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderWindowQuadsInternal(params->context, params->world_binding,
                                 contents_agent, windowskin_agent, padding_rect,
                                 params->scissors);
  }
}

void Window2Impl::GPUCreateWindowInternal() {
  agent_.batch = renderer::QuadBatch::Make(**context()->render_device);

  auto& pipelines = context()->render.pipeline_loader;
  agent_.flat_binding = pipelines->viewport.CreateBinding();
  agent_.base_binding = pipelines->base.CreateBinding();
  agent_.background_binding = pipelines->base.CreateBinding();
  agent_.controls_binding = pipelines->base.CreateBinding();
  agent_.content_binding = pipelines->base.CreateBinding();

  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::Binding_Flat::Params),
      "window2.uniform", &agent_.uniform, Diligent::USAGE_DEFAULT);
}

void Window2Impl::GPUCompositeWindowQuadsInternal(
    Diligent::IDeviceContext* render_context,
    GPUBitmapData* contents,
    GPUBitmapData* windowskin,
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
    renderer::MakeProjectionMatrix(world_matrix.projection,
                                   bound_.Size().Recast<float>());
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
  const base::Vec2i& draw_offset = bound_.Position();
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
  int32_t frames_quads_count = 0;

  agent_.cursor_quad_offset = 0;
  agent_.cursor_draw_count = 0;

  if (windowskin) {
    const base::Rect background1_src(0, 0, 32 * scale_, 32 * scale_);
    const base::Rect background2_src(0, 32 * scale_, 32 * scale_, 32 * scale_);
    const base::Rect background_dest(scale_, scale_, bound_.width - 2 * scale_,
                                     bound_.height - 2 * scale_);

    // Stretch layer (1)
    renderer::Quad::SetPositionRect(&quads[quad_index], background_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], background1_src,
                                    windowskin->size.Recast<float>());
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
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index], corner_right_top_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_top_src,
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    corner_left_bottom_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_bottom_src,
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    corner_right_bottom_dest);
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_bottom_src,
                                    windowskin->size.Recast<float>());
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

  // Background texture display quad (1)
  const base::Rect background_pos(
      bound_.x,
      static_cast<float>(bound_.y) +
          static_cast<float>(bound_.height / 2.0f) * (1.0f - openness_norm),
      bound_.width, bound_.height * openness_norm);
  renderer::Quad::SetPositionRect(&quads[quad_index], background_pos);
  renderer::Quad::SetTexCoordRectNorm(
      &quads[quad_index], base::RectF(base::Vec2(0), base::Vec2(1)));
  renderer::Quad::SetColor(&quads[quad_index], base::Vec4(1));
  agent_.background_quad_offset = quad_index;
  quad_index++;

  // Controls & Contents
  if (openness_ >= 255) {
    // Controls
    if (windowskin) {
      agent_.controls_quad_offset = quad_index;
      agent_.controls_draw_count = 0;
      const base::Vec2i arrow_display_offset =
          draw_offset + ((bound_.Size() - base::Vec2i(8 * scale_)) / 2);

      // Arrows (4)
      if (arrows_visible_) {
        const base::Rect arrow_up_dest(arrow_display_offset.x,
                                       draw_offset.y + 2 * scale_, 8 * scale_,
                                       4 * scale_);
        const base::Rect arrow_down_dest(
            arrow_display_offset.x, draw_offset.y + bound_.height - 6 * scale_,
            8 * scale_, 4 * scale_);
        const base::Rect arrow_left_dest(draw_offset.x + 2 * scale_,
                                         arrow_display_offset.y, 4 * scale_,
                                         8 * scale_);
        const base::Rect arrow_right_dest(
            draw_offset.x + bound_.width - 6 * scale_, arrow_display_offset.y,
            4 * scale_, 8 * scale_);

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
                                            windowskin->size.Recast<float>());
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            agent_.controls_draw_count++;
          }
          if (origin_.y > 0) {
            renderer::Quad::SetPositionRect(&quads[quad_index], arrow_up_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_up_src,
                                            windowskin->size.Recast<float>());
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            agent_.controls_draw_count++;
          }
          if (padding_rect.width < (contents->size.x - origin_.x)) {
            renderer::Quad::SetPositionRect(&quads[quad_index],
                                            arrow_right_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_right_src,
                                            windowskin->size.Recast<float>());
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            agent_.controls_draw_count++;
          }
          if (padding_rect.height < (contents->size.y - origin_.y)) {
            renderer::Quad::SetPositionRect(&quads[quad_index],
                                            arrow_down_dest);
            renderer::Quad::SetTexCoordRect(&quads[quad_index], arrow_down_src,
                                            windowskin->size.Recast<float>());
            renderer::Quad::SetColor(&quads[quad_index],
                                     base::Vec4(contents_opacity_norm));
            quad_index++;
            agent_.controls_draw_count++;
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
        const base::Rect pause_dest(draw_offset.x + arrow_display_offset.x,
                                    draw_offset.y + bound_.height - 8 * scale_,
                                    8 * scale_, 8 * scale_);
        renderer::Quad::SetPositionRect(&quads[quad_index], pause_dest);
        renderer::Quad::SetTexCoordRect(&quads[quad_index],
                                        pause_src[pause_index_ / 8],
                                        windowskin->size.Recast<float>());
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
        agent_.controls_draw_count++;
      }

      // Cursor (9)
      const auto cursor_rect = cursor_rect_->AsBaseRect();
      if (cursor_rect.width > 0 && cursor_rect.height > 0) {
        auto build_cursor_internal = [&](const base::Rect& rect, int32_t unit,
                                         base::Rect quad_rects[9]) {
          const int32_t w = rect.width;
          const int32_t h = rect.height;
          const int32_t l = rect.x;
          const int32_t r = l + w;
          const int32_t t = rect.y;
          const int32_t b = t + h;

          int32_t i = 0;
          quad_rects[i++] = {{l, t}, base::Vec2i(unit)};         // Left-Top
          quad_rects[i++] = {{r - unit, t}, base::Vec2i(unit)};  // Right-Top
          quad_rects[i++] = {{r - unit, b - unit},
                             base::Vec2i(unit)};                 // Right-Bottom
          quad_rects[i++] = {{l, b - unit}, base::Vec2i(unit)};  // Left-Bottom

          quad_rects[i++] = {l, t + unit, unit, h - unit * 2};         // Left
          quad_rects[i++] = {r - unit, t + unit, unit, h - unit * 2};  // Right
          quad_rects[i++] = {l + unit, t, w - unit * 2, unit};         // Top
          quad_rects[i++] = {l + unit, b - unit, w - unit * 2, unit};  // Bottom
          quad_rects[i++] = {l + unit, t + unit, w - unit * 2,
                             h - unit * 2};  // Center
        };

        auto build_cursor_quads = [&](const base::Rect& src,
                                      const base::Rect& dst,
                                      renderer::Quad vert[9]) {
          const int32_t cursor_scale = scale_ >= 4 ? scale_ * 2 : 4;

          base::Rect texcoords[9], positions[9];
          const base::Vec4 draw_color(cursor_opacity_norm *
                                      contents_opacity_norm);
          build_cursor_internal(src, cursor_scale, texcoords);
          build_cursor_internal(dst, cursor_scale, positions);

          for (int32_t i = 0; i < 9; ++i) {
            renderer::Quad::SetPositionRect(&vert[i], positions[i]);
            renderer::Quad::SetTexCoordRect(&vert[i], texcoords[i],
                                            windowskin->size.Recast<float>());
            renderer::Quad::SetColor(&vert[i], draw_color);
          }

          return 9;
        };

        const base::Rect cursor_src(32 * scale_, 32 * scale_, 16 * scale_,
                                    16 * scale_);
        if (cursor_rect.width > 0 && cursor_rect.height > 0) {
          base::Rect cursor_dest(
              draw_offset + padding_rect.Position() + cursor_rect.Position(),
              cursor_rect.Size());

          if (rgss3_style_) {
            cursor_dest.x -= origin_.x;
            cursor_dest.y -= origin_.y;
          }

          const int32_t cursor_draw_count =
              build_cursor_quads(cursor_src, cursor_dest, &quads[quad_index]);
          agent_.cursor_quad_offset = quad_index;
          agent_.cursor_draw_count = cursor_draw_count;
          quad_index += cursor_draw_count;
        }
      }
    }

    // Contents (1)
    if (contents) {
      const base::Rect content_region(
          draw_offset + padding_rect.Position() - origin_, contents->size);

      renderer::Quad::SetPositionRect(&quads[quad_index], content_region);
      renderer::Quad::SetTexCoordRectNorm(
          &quads[quad_index], base::RectF(base::Vec2(0), base::Vec2(1)));
      renderer::Quad::SetColor(&quads[quad_index],
                               base::Vec4(contents_opacity_norm));
      agent_.contents_quad_offset = quad_index;
      quad_index++;
    }
  }

  // Update GPU vertex buffer
  agent_.batch.QueueWrite(render_context, quads.data(), quads.size());

  // Make sure index buffer count
  context()->render.quad_index->Allocate(quad_index);

  // Update background tone uniform buffer
  renderer::Binding_Flat::Params uniform;
  uniform.Color = base::Vec4();
  uniform.Tone = tone_->AsNormColor();
  render_context->UpdateBuffer(
      agent_.uniform, 0, sizeof(uniform), &uniform,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Prepare render pipelines
  auto* pipeline_tone =
      context()->render.pipeline_states->window2.viewport.RawPtr();
  auto* pipeline_tone_alpha =
      context()->render.pipeline_states->window2.viewport_alpha.RawPtr();
  auto* pipeline_base =
      context()->render.pipeline_states->window2.base.RawPtr();

  // Setup render pass
  auto* render_target_view =
      agent_.texture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearRenderTarget(
      render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup shader binding
  if (windowskin) {
    agent_.flat_binding.u_transform->Set(agent_.world);
    agent_.flat_binding.u_texture->Set(windowskin->resource);
    agent_.flat_binding.u_params->Set(agent_.uniform);

    agent_.base_binding.u_transform->Set(agent_.world);
    agent_.base_binding.u_texture->Set(windowskin->resource);

    // Apply global vertex and index
    Diligent::IBuffer* const vertex_buffer = *agent_.batch;
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render.quad_index, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Reset scissor test
    Diligent::Rect render_scissor(0, 0, bound_.width, bound_.height);
    render_context->SetScissorRects(1, &render_scissor, UINT32_MAX, UINT32_MAX);

    int32_t quads_draw_offset = 0;

    // Stretch layer pass
    {
      render_context->SetPipelineState(pipeline_tone);
      render_context->CommitShaderResources(
          *agent_.flat_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      render_context->DrawIndexed(draw_indexed_attribs);
      quads_draw_offset++;
    }

    // Tiled layer pass
    {
      render_context->SetPipelineState(pipeline_tone_alpha);
      render_context->CommitShaderResources(
          *agent_.flat_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = tiled_quads_count * 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = quads_draw_offset * 6;
      render_context->DrawIndexed(draw_indexed_attribs);
      quads_draw_offset += tiled_quads_count;
    }

    // Corners & Frames
    render_context->SetPipelineState(pipeline_base);
    render_context->CommitShaderResources(
        *agent_.base_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = frames_quads_count * 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = quads_draw_offset * 6;
      render_context->DrawIndexed(draw_indexed_attribs);
    }
  }
}

void Window2Impl::GPURenderWindowQuadsInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::IBuffer* world_binding,
    GPUBitmapData* contents,
    GPUBitmapData* windowskin,
    const base::Rect& padding_rect,
    ScissorStack* scissor_stack) {
  auto* pipeline = context()->render.pipeline_states->window.RawPtr();

  // Setup shader binding
  agent_.background_binding.u_transform->Set(world_binding);
  agent_.background_binding.u_texture->Set(
      agent_.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.controls_binding.u_transform->Set(world_binding);
  agent_.controls_binding.u_texture->Set(windowskin->resource);
  agent_.content_binding.u_transform->Set(world_binding);
  agent_.content_binding.u_texture->Set(contents->resource);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.batch;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render.quad_index, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Background render pass
  if (windowskin) {
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *agent_.background_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType =
        context()->render.quad_index->GetIndexType();
    draw_indexed_attribs.FirstIndexLocation = agent_.background_quad_offset * 6;
    render_context->DrawIndexed(draw_indexed_attribs);
  }

  if (openness_ >= 255) {
    // Controls render pass
    if (windowskin && agent_.controls_draw_count) {
      render_context->SetPipelineState(pipeline);
      render_context->CommitShaderResources(
          *agent_.controls_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = agent_.controls_draw_count * 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = agent_.controls_quad_offset * 6;
      render_context->DrawIndexed(draw_indexed_attribs);
    }

    const auto current_viewport = node_.GetParentViewport()->bound.Position() -
                                  node_.GetParentViewport()->origin;
    const base::Rect clip_region(
        current_viewport + bound_.Position() + padding_rect.Position(),
        padding_rect.Size());
    if (scissor_stack->Push(clip_region)) {
      // Cursor pass
      if (windowskin && agent_.cursor_draw_count) {
        render_context->SetPipelineState(pipeline);
        render_context->CommitShaderResources(
            *agent_.controls_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Execute render command
        Diligent::DrawIndexedAttribs draw_indexed_attribs;
        draw_indexed_attribs.NumIndices = agent_.cursor_draw_count * 6;
        draw_indexed_attribs.IndexType =
            context()->render.quad_index->GetIndexType();
        draw_indexed_attribs.FirstIndexLocation = agent_.cursor_quad_offset * 6;
        render_context->DrawIndexed(draw_indexed_attribs);
      }

      // Contents render pass
      if (contents) {
        render_context->SetPipelineState(pipeline);
        render_context->CommitShaderResources(
            *agent_.content_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Execute render command
        Diligent::DrawIndexedAttribs draw_indexed_attribs;
        draw_indexed_attribs.NumIndices = 6;
        draw_indexed_attribs.IndexType =
            context()->render.quad_index->GetIndexType();
        draw_indexed_attribs.FirstIndexLocation =
            agent_.contents_quad_offset * 6;
        render_context->DrawIndexed(draw_indexed_attribs);
      }

      // Restore scissor region
      scissor_stack->Pop();
    }
  }
}

}  // namespace content
