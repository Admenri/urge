// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window_impl.h"

#include "content/render/tilequad.h"

namespace content {

scoped_refptr<Window> Window::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  int32_t scale_,
                                  ExceptionState& exception_state) {
  return base::MakeRefCounted<WindowImpl>(
      execution_context, ViewportImpl::From(viewport), std::max(1, scale_));
}

WindowImpl::WindowImpl(ExecutionContext* execution_context,
                       scoped_refptr<ViewportImpl> parent,
                       int32_t scale_)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      background_node_(parent ? parent->GetDrawableController()
                              : execution_context->screen_drawable_node,
                       SortKey()),
      control_node_(parent ? parent->GetDrawableController()
                           : execution_context->screen_drawable_node,
                    SortKey(2)),
      scale_(scale_),
      viewport_(parent),
      cursor_rect_(base::MakeRefCounted<RectImpl>(base::Rect())) {
  background_node_.RegisterEventHandler(base::BindRepeating(
      &WindowImpl::BackgroundNodeHandlerInternal, base::Unretained(this)));
  control_node_.RegisterEventHandler(base::BindRepeating(
      &WindowImpl::ControlNodeHandlerInternal, base::Unretained(this)));

  GPUCreateWindowInternal();
}

WindowImpl::~WindowImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void WindowImpl::SetLabel(const base::String& label,
                          ExceptionState& exception_state) {
  background_node_.SetDebugLabel(label);
  control_node_.SetDebugLabel(label);
}

void WindowImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool WindowImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void WindowImpl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  ++pause_index_;
  if (pause_index_ >= 32)
    pause_index_ = 0;

  if (active_) {
    cursor_opacity_ += cursor_fade_ ? -8 : 8;
    if (cursor_opacity_ > 255) {
      cursor_opacity_ = 255;
      cursor_fade_ = true;
    } else if (cursor_opacity_ < 128) {
      cursor_opacity_ = 128;
      cursor_fade_ = false;
    }
  } else {
    cursor_opacity_ = 128;
  }
}

scoped_refptr<Viewport> WindowImpl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void WindowImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  DrawNodeController* controller = viewport_
                                       ? viewport_->GetDrawableController()
                                       : context()->screen_drawable_node;
  background_node_.RebindController(controller);
  control_node_.RebindController(controller);
}

scoped_refptr<Bitmap> WindowImpl::Get_Windowskin(
    ExceptionState& exception_state) {
  return windowskin_;
}

void WindowImpl::Put_Windowskin(const scoped_refptr<Bitmap>& value,
                                ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  windowskin_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Bitmap> WindowImpl::Get_Contents(
    ExceptionState& exception_state) {
  return contents_;
}

void WindowImpl::Put_Contents(const scoped_refptr<Bitmap>& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_ = CanvasImpl::FromBitmap(value);
}

bool WindowImpl::Get_Stretch(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return stretch_;
}

void WindowImpl::Put_Stretch(const bool& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  stretch_ = value;
}

scoped_refptr<Rect> WindowImpl::Get_CursorRect(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return cursor_rect_;
}

void WindowImpl::Put_CursorRect(const scoped_refptr<Rect>& value,
                                ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *cursor_rect_ = *RectImpl::From(value);
}

bool WindowImpl::Get_Active(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return active_;
}

void WindowImpl::Put_Active(const bool& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  active_ = value;
}

bool WindowImpl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return background_node_.GetVisibility();
}

void WindowImpl::Put_Visible(const bool& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  background_node_.SetNodeVisibility(value);
  control_node_.SetNodeVisibility(value);
}

bool WindowImpl::Get_Pause(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return pause_;
}

void WindowImpl::Put_Pause(const bool& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  pause_ = value;
}

int32_t WindowImpl::Get_X(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.x;
}

void WindowImpl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.x = value;
}

int32_t WindowImpl::Get_Y(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.y;
}

void WindowImpl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.y = value;
}

int32_t WindowImpl::Get_Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.width;
}

void WindowImpl::Put_Width(const int32_t& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.width = value;
}

int32_t WindowImpl::Get_Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.height;
}

void WindowImpl::Put_Height(const int32_t& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.height = value;
}

int32_t WindowImpl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return background_node_.GetSortKeys()->weight[0];
}

void WindowImpl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  background_node_.SetNodeSortWeight(value);
  control_node_.SetNodeSortWeight(value + 2);
}

int32_t WindowImpl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.x;
}

void WindowImpl::Put_Ox(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.x = value;
}

int32_t WindowImpl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.y;
}

void WindowImpl::Put_Oy(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.y = value;
}

int32_t WindowImpl::Get_Opacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return opacity_;
}

void WindowImpl::Put_Opacity(const int32_t& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t WindowImpl::Get_BackOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return back_opacity_;
}

void WindowImpl::Put_BackOpacity(const int32_t& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  back_opacity_ = std::clamp(value, 0, 255);
}

int32_t WindowImpl::Get_ContentsOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return contents_opacity_;
}

void WindowImpl::Put_ContentsOpacity(const int32_t& value,
                                     ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_opacity_ = std::clamp(value, 0, 255);
}

void WindowImpl::OnObjectDisposed() {
  background_node_.DisposeNode();
  control_node_.DisposeNode();

  Agent empty_agent;
  std::swap(agent_, empty_agent);
}

void WindowImpl::BackgroundNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  if (windowskin_ && windowskin_->GetAgent()) {
    if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
      GPUCompositeBackgroundLayerInternal(params->context,
                                          windowskin_->GetAgent());
    } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
      GPURenderBackgroundLayerInternal(params->context, params->world_binding,
                                       params->viewport, params->origin,
                                       windowskin_->GetAgent());
    }
  }
}

void WindowImpl::ControlNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  CanvasImpl::Agent* windowskin_agent =
      windowskin_ ? windowskin_->GetAgent() : nullptr;
  CanvasImpl::Agent* contents_agent =
      contents_ ? contents_->GetAgent() : nullptr;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeControlLayerInternal(params->context, windowskin_agent,
                                     contents_agent);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderControlLayerInternal(params->context, params->world_binding,
                                  params->viewport, params->origin,
                                  windowskin_agent, contents_agent);
  }
}

void WindowImpl::GPUCreateWindowInternal() {
  agent_.background_batch =
      renderer::QuadBatch::Make(**context()->render_device);
  agent_.control_batch = renderer::QuadBatch::Make(**context()->render_device);
  agent_.shader_binding =
      context()->render_device->GetPipelines()->base.CreateBinding();
}

void WindowImpl::GPUCompositeBackgroundLayerInternal(
    renderer::RenderContext* render_context,
    CanvasImpl::Agent* windowskin) {
  auto& quad_buffer = agent_.background_cache;

  // Display offset
  base::Vec2i display_offset = bound_.Position();

  // Calculate total quads count
  {
    // Corners
    int32_t count = 4;

    // Background
    if (stretch_) {
      count += 1;
    } else {
      int32_t horizon_count =
          CalculateQuadTileCount(64 * scale_, bound_.width - 2 * scale_);
      int32_t vertical_count =
          CalculateQuadTileCount(64 * scale_, bound_.height - 2 * scale_);
      count += horizon_count * vertical_count;
    }

    // Frame tiles
    const int32_t horizon_frame_size = bound_.width - scale_ * 16;
    const int32_t vertical_frame_size = bound_.height - scale_ * 16;
    count += CalculateQuadTileCount(16 * scale_, horizon_frame_size) * 2;
    count += CalculateQuadTileCount(16 * scale_, vertical_frame_size) * 2;

    // Setup vertex buffer
    quad_buffer.resize(count);
  }

  renderer::Quad* quad_ptr = quad_buffer.data();
  float opacity_norm = static_cast<float>(opacity_) / 255.0f;
  float back_opacity_norm = static_cast<float>(back_opacity_) / 255.0f;

  // Gen quadangle vertices
  {
    // Stretch / Tile layer
    base::Rect dest_rect(display_offset.x + scale_, display_offset.y + scale_,
                         bound_.width - 2 * scale_, bound_.height - 2 * scale_);
    const base::Rect background_src(0, 0, 64 * scale_, 64 * scale_);

    int32_t background_quad_count = 0;
    base::Vec4 background_opacity_norm(opacity_norm * back_opacity_norm);
    if (stretch_) {
      renderer::Quad::SetPositionRect(quad_ptr, dest_rect);
      renderer::Quad::SetTexCoordRect(quad_ptr, background_src,
                                      windowskin->size);
      renderer::Quad::SetColor(quad_ptr, background_opacity_norm);
      background_quad_count += 1;
    } else {
      // Build tiled background quads
      background_quad_count +=
          BuildTiles(background_src, dest_rect, background_opacity_norm,
                     windowskin->size, quad_ptr);
    }

    quad_ptr += background_quad_count;

    // Frame Corners
    base::Rect corner_left_top(64 * scale_, 0, 8 * scale_, 8 * scale_);
    base::Rect corner_right_top(88 * scale_, 0, 8 * scale_, 8 * scale_);
    base::Rect corner_right_bottom(88 * scale_, 24 * scale_, 8 * scale_,
                                   8 * scale_);
    base::Rect corner_left_bottom(64 * scale_, 24 * scale_, 8 * scale_,
                                  8 * scale_);

    renderer::Quad::SetPositionRect(
        quad_ptr,
        base::Rect(display_offset.x, display_offset.y, 8 * scale_, 8 * scale_));
    renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_top,
                                    windowskin->size);
    renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity_norm));
    ++quad_ptr;
    renderer::Quad::SetPositionRect(
        quad_ptr, base::Rect(display_offset.x + bound_.width - 8 * scale_,
                             display_offset.y, 8 * scale_, 8 * scale_));
    renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_top,
                                    windowskin->size);
    renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity_norm));
    ++quad_ptr;
    renderer::Quad::SetPositionRect(
        quad_ptr, base::Rect(display_offset.x + bound_.width - 8 * scale_,
                             display_offset.y + bound_.height - 8 * scale_,
                             8 * scale_, 8 * scale_));
    renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_bottom,
                                    windowskin->size);
    renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity_norm));
    ++quad_ptr;
    renderer::Quad::SetPositionRect(
        quad_ptr, base::Rect(display_offset.x,
                             display_offset.y + bound_.height - 8 * scale_,
                             8 * scale_, 8 * scale_));
    renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_bottom,
                                    windowskin->size);
    renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity_norm));
    ++quad_ptr;

    // Frame tiles
    base::Rect frame_up(72 * scale_, 0, 16 * scale_, 8 * scale_);
    base::Rect frame_down(72 * scale_, 24 * scale_, 16 * scale_, 8 * scale_);
    base::Rect frame_left(64 * scale_, 8 * scale_, 8 * scale_, 16 * scale_);
    base::Rect frame_right(88 * scale_, 8 * scale_, 8 * scale_, 16 * scale_);

    base::Vec2i frame_up_pos(display_offset.x + 8 * scale_, display_offset.y);
    base::Vec2i frame_down_pos(display_offset.x + 8 * scale_,
                               display_offset.y + bound_.height - 8 * scale_);
    base::Vec2i frame_left_pos(display_offset.x, display_offset.y + 8 * scale_);
    base::Vec2i frame_right_pos(display_offset.x + bound_.width - 8 * scale_,
                                display_offset.y + 8 * scale_);

    quad_ptr += BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, frame_up, frame_up_pos, base::Vec4(opacity_norm),
        bound_.width - 16 * scale_, windowskin->size, quad_ptr);
    quad_ptr += BuildTilesAlongAxis(TileAxis::HORIZONTAL, frame_down,
                                    frame_down_pos, base::Vec4(opacity_norm),
                                    bound_.width - 16 * scale_,
                                    windowskin->size, quad_ptr);
    quad_ptr += BuildTilesAlongAxis(TileAxis::VERTICAL, frame_left,
                                    frame_left_pos, base::Vec4(opacity_norm),
                                    bound_.height - 16 * scale_,
                                    windowskin->size, quad_ptr);
    quad_ptr += BuildTilesAlongAxis(TileAxis::VERTICAL, frame_right,
                                    frame_right_pos, base::Vec4(opacity_norm),
                                    bound_.height - 16 * scale_,
                                    windowskin->size, quad_ptr);
  }

  agent_.background_batch.QueueWrite(**render_context, quad_buffer.data(),
                                     quad_buffer.size());
}

void WindowImpl::GPUCompositeControlLayerInternal(
    renderer::RenderContext* render_context,
    CanvasImpl::Agent* windowskin,
    CanvasImpl::Agent* contents) {
  const float contents_opacity_norm = contents_opacity_ / 255.0f;
  const float cursor_opacity_norm = cursor_opacity_ / 255.0f;

  // Cursor render
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
    quad_rects[i++] = base::Rect(x2 - scale_, y2 - scale_, scale_, scale_);
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
    quad_rects[i++] =
        base::Rect(x1 + scale_, y1 + scale_, w - scale_ * 2, h - scale_ * 2);
  };

  auto build_cursor_quads = [&](const base::Rect& src, const base::Rect& dst,
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

  auto& quad_buffer = agent_.control_cache;
  quad_buffer.reserve(9 + 4 + 1 + 1);
  quad_buffer.clear();
  base::Vec2i display_offset = bound_.Position();

  if (windowskin) {
    const auto cursor_rect = cursor_rect_->AsBaseRect();
    // Cursor render (9 Quads)
    base::Rect cursor_dest_rect(display_offset.x + cursor_rect.x + 8 * scale_,
                                display_offset.y + cursor_rect.y + 8 * scale_,
                                cursor_rect.width, cursor_rect.height);
    if (cursor_dest_rect.width > 0 && cursor_dest_rect.height > 0) {
      base::Rect cursor_src(64 * scale_, 32 * scale_, 16 * scale_, 16 * scale_);

      renderer::Quad cursors_quads[9];
      build_cursor_quads(cursor_src, cursor_dest_rect, cursors_quads);
      quad_buffer.insert(quad_buffer.end(), std::begin(cursors_quads),
                         std::end(cursors_quads));
    }

    // Arrows render (0-4 Quads)
    const base::Vec2i scroll =
        display_offset +
        (bound_.Size() - base::Vec2i(8 * scale_)) / base::Vec2i(2);
    base::Rect scroll_arrow_up_pos =
        base::Rect(scroll.x, bound_.y + 2 * scale_, 8 * scale_, 4 * scale_);
    base::Rect scroll_arrow_down_pos =
        base::Rect(scroll.x, bound_.y + bound_.height - 6 * scale_, 8 * scale_,
                   4 * scale_);
    base::Rect scroll_arrow_left_pos =
        base::Rect(bound_.x + 2 * scale_, scroll.y, 4 * scale_, 8 * scale_);
    base::Rect scroll_arrow_right_pos = base::Rect(
        bound_.x + bound_.width - 6 * scale_, scroll.y, 4 * scale_, 8 * scale_);

    if (contents) {
      base::Rect scroll_arrow_up_src = {76 * scale_, 8 * scale_, 8 * scale_,
                                        4 * scale_};
      base::Rect scroll_arrow_down_src = {76 * scale_, 20 * scale_, 8 * scale_,
                                          4 * scale_};
      base::Rect scroll_arrow_left_src = {72 * scale_, 12 * scale_, 4 * scale_,
                                          8 * scale_};
      base::Rect scroll_arrow_right_src = {84 * scale_, 12 * scale_, 4 * scale_,
                                           8 * scale_};

      if (origin_.x > 0) {
        renderer::Quad quad;
        renderer::Quad::SetPositionRect(&quad, scroll_arrow_left_pos);
        renderer::Quad::SetTexCoordRect(&quad, scroll_arrow_left_src,
                                        windowskin->size);
        renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
        quad_buffer.push_back(quad);
      }

      if (origin_.y > 0) {
        renderer::Quad quad;
        renderer::Quad::SetPositionRect(&quad, scroll_arrow_up_pos);
        renderer::Quad::SetTexCoordRect(&quad, scroll_arrow_up_src,
                                        windowskin->size);
        renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
        quad_buffer.push_back(quad);
      }

      if ((bound_.width - 16 * scale_) < (contents->size.x - origin_.x)) {
        renderer::Quad quad;
        renderer::Quad::SetPositionRect(&quad, scroll_arrow_right_pos);
        renderer::Quad::SetTexCoordRect(&quad, scroll_arrow_right_src,
                                        windowskin->size);
        renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
        quad_buffer.push_back(quad);
      }

      if ((bound_.height - 16 * scale_) < (contents->size.y - origin_.y)) {
        renderer::Quad quad;
        renderer::Quad::SetPositionRect(&quad, scroll_arrow_down_pos);
        renderer::Quad::SetTexCoordRect(&quad, scroll_arrow_down_src,
                                        windowskin->size);
        renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
        quad_buffer.push_back(quad);
      }
    }

    // Pause render (0-1 Quads)
    base::Rect pause_animation[] = {
        {80 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
        {88 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
        {80 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
        {88 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
    };

    if (pause_) {
      renderer::Quad quad;
      renderer::Quad::SetPositionRect(
          &quad, base::Rect(display_offset.x + (bound_.width - 8 * scale_) / 2,
                            display_offset.y + bound_.height - 8 * scale_,
                            8 * scale_, 8 * scale_));
      renderer::Quad::SetTexCoordRect(&quad, pause_animation[pause_index_ / 8],
                                      windowskin->size);
      renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
      quad_buffer.push_back(quad);
    }
  }

  // Setup control draw count
  agent_.control_draw_count = quad_buffer.size();

  // Render contents (0-1 Quads)
  if (contents) {
    base::Vec2i content_position = display_offset;
    content_position.x += 8 * scale_ - origin_.x;
    content_position.y += 8 * scale_ - origin_.y;

    renderer::Quad quad;
    renderer::Quad::SetPositionRect(
        &quad, base::Rect(content_position, contents->size));
    renderer::Quad::SetTexCoordRect(&quad, base::Rect(contents->size),
                                    contents->size);
    renderer::Quad::SetColor(&quad, base::Vec4(contents_opacity_));
    quad_buffer.push_back(quad);

    agent_.contents_draw_count = 1;
  } else {
    // Disable contents render pass
    agent_.contents_draw_count = 0;
  }

  agent_.control_batch.QueueWrite(**render_context, quad_buffer.data(),
                                  quad_buffer.size());
}

void WindowImpl::GPURenderBackgroundLayerInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    const base::Rect& last_viewport,
    const base::Vec2i& last_origin,
    CanvasImpl::Agent* windowskin) {
  if (agent_.background_cache.size()) {
    auto& pipeline_set = context()->render_device->GetPipelines()->base;
    auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL);

    const base::Rect window_bound(last_viewport.x + bound_.x - last_origin.x,
                                  last_viewport.y + bound_.y - last_origin.y,
                                  bound_.width, bound_.height);
    auto interact_region = base::MakeIntersect(last_viewport, window_bound);
    if (!interact_region.width || !interact_region.height)
      return;

    render_context->ScissorState()->Push(interact_region);

    // Setup uniform params
    agent_.shader_binding.u_transform->Set(world_binding);
    agent_.shader_binding.u_texture->Set(windowskin->resource);

    // Apply pipeline state
    (*render_context)->SetPipelineState(pipeline);
    (*render_context)
        ->CommitShaderResources(
            *agent_.shader_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.background_batch;
    (*render_context)
        ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (*render_context)
        ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6 * agent_.background_cache.size();
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    (*render_context)->DrawIndexed(draw_indexed_attribs);

    render_context->ScissorState()->Pop();
  }
}

void WindowImpl::GPURenderControlLayerInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    const base::Rect& last_viewport,
    const base::Vec2i& last_origin,
    CanvasImpl::Agent* windowskin,
    CanvasImpl::Agent* contents) {
  if (agent_.control_draw_count || agent_.contents_draw_count) {
    auto& pipeline_set = context()->render_device->GetPipelines()->base;
    auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL);

    {
      const base::Rect window_bound(last_viewport.x + bound_.x - last_origin.x,
                                    last_viewport.y + bound_.y - last_origin.y,
                                    bound_.width, bound_.height);
      const auto interact_region =
          base::MakeIntersect(last_viewport, window_bound);
      if (!interact_region.width || !interact_region.height)
        return;

      render_context->ScissorState()->Push(interact_region);
    }

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.control_batch;
    (*render_context)
        ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (*render_context)
        ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (windowskin && agent_.control_draw_count) {
      // Setup uniform params
      agent_.shader_binding.u_transform->Set(world_binding);
      agent_.shader_binding.u_texture->Set(windowskin->resource);

      // Apply pipeline state
      (*render_context)->SetPipelineState(pipeline);
      (*render_context)
          ->CommitShaderResources(
              *agent_.shader_binding,
              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6 * agent_.control_draw_count;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
    }

    {
      base::Rect inbox_region = bound_;
      inbox_region.x += last_viewport.x + 8 * scale_ - last_origin.x;
      inbox_region.y += last_viewport.y + 8 * scale_ - last_origin.y;
      inbox_region.width -= 16 * scale_;
      inbox_region.height -= 16 * scale_;

      auto interact_region = base::MakeIntersect(last_viewport, inbox_region);
      if (!interact_region.width || !interact_region.height)
        return;

      render_context->ScissorState()->Apply(interact_region);
    }

    if (contents && agent_.contents_draw_count) {
      // Setup uniform params
      agent_.shader_binding.u_transform->Set(world_binding);
      agent_.shader_binding.u_texture->Set(contents->resource);

      // Apply pipeline state
      (*render_context)->SetPipelineState(pipeline);
      (*render_context)
          ->CommitShaderResources(
              *agent_.shader_binding,
              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6 * agent_.contents_draw_count;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = agent_.control_draw_count * 6;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
    }

    render_context->ScissorState()->Pop();
  }
}

}  // namespace content
