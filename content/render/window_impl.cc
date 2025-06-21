// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window_impl.h"

#include "content/render/tilequad.h"
#include "renderer/utils/texture_utils.h"

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

  BitmapAgent* windowskin_agent =
      windowskin_ ? windowskin_->GetAgent() : nullptr;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeBackgroundLayerInternal(params->context, windowskin_agent);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderBackgroundLayerInternal(params->context, params->world_binding,
                                     windowskin_agent);
  }
}

void WindowImpl::ControlNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  BitmapAgent* windowskin_agent =
      windowskin_ ? windowskin_->GetAgent() : nullptr;
  BitmapAgent* contents_agent = contents_ ? contents_->GetAgent() : nullptr;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeControlLayerInternal(params->context, windowskin_agent,
                                     contents_agent);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderControlLayerInternal(params->context, params->world_binding,
                                  windowskin_agent, contents_agent);
  }
}

void WindowImpl::GPUCreateWindowInternal() {
  agent_.background_batch =
      renderer::QuadBatch::Make(**context()->render_device);
  agent_.controls_batch = renderer::QuadBatch::Make(**context()->render_device);

  agent_.base_binding =
      context()->render_device->GetPipelines()->base.CreateBinding();
  agent_.content_binding =
      context()->render_device->GetPipelines()->base.CreateBinding();
}

void WindowImpl::GPUCompositeBackgroundLayerInternal(
    renderer::RenderContext* render_context,
    BitmapAgent* windowskin) {
  const base::Vec2i& draw_offset = bound_.Position();

  // Generate background quads
  int32_t quad_index = 0;
  if (windowskin) {
    // Corners
    int32_t quad_count = 4;

    // Background
    if (stretch_) {
      quad_count += 1;
    } else {
      int32_t horizon_count =
          CalculateQuadTileCount(64 * scale_, bound_.width - 2 * scale_);
      int32_t vertical_count =
          CalculateQuadTileCount(64 * scale_, bound_.height - 2 * scale_);
      quad_count += horizon_count * vertical_count;
    }

    // Frame tiles
    const int32_t horizon_frame_size = bound_.width - scale_ * 16;
    const int32_t vertical_frame_size = bound_.height - scale_ * 16;
    quad_count += CalculateQuadTileCount(16 * scale_, horizon_frame_size) * 2;
    quad_count += CalculateQuadTileCount(16 * scale_, vertical_frame_size) * 2;

    // Resize cache
    auto& quads = agent_.background_cache;
    quads.resize(quad_count);

    // Draw attributes
    const float opacity_norm = static_cast<float>(opacity_) / 255.0f;
    const float back_opacity_norm = static_cast<float>(back_opacity_) / 255.0f;

    // Stretch / Tile layer
    const base::Rect dest_rect(draw_offset.x + scale_, draw_offset.y + scale_,
                               bound_.width - 2 * scale_,
                               bound_.height - 2 * scale_);
    const base::Rect background_src(0, 0, 64 * scale_, 64 * scale_);

    if (stretch_) {
      renderer::Quad::SetPositionRect(&quads[quad_index], dest_rect);
      renderer::Quad::SetTexCoordRect(&quads[quad_index], background_src,
                                      windowskin->size);
      renderer::Quad::SetColor(&quads[quad_index],
                               base::Vec4(opacity_norm * back_opacity_norm));
      quad_index++;
    } else {
      // Build tiled background quads
      quad_index += BuildTiles(background_src, dest_rect,
                               base::Vec4(opacity_norm * back_opacity_norm),
                               windowskin->size, &quads[quad_index]);
    }

    // Frame Corners
    base::Rect corner_left_top(64 * scale_, 0, 8 * scale_, 8 * scale_);
    base::Rect corner_right_top(88 * scale_, 0, 8 * scale_, 8 * scale_);
    base::Rect corner_right_bottom(88 * scale_, 24 * scale_, 8 * scale_,
                                   8 * scale_);
    base::Rect corner_left_bottom(64 * scale_, 24 * scale_, 8 * scale_,
                                  8 * scale_);

    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    base::Rect{draw_offset, 8 * scale_});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_top,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x + bound_.width - 8 * scale_, draw_offset.y},
                   8 * scale_});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_top,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x + bound_.width - 8 * scale_,
                    draw_offset.y + bound_.height - 8 * scale_},
                   8 * scale_});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_bottom,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x, draw_offset.y + bound_.height - 8 * scale_},
                   8 * scale_});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_bottom,
                                    windowskin->size);
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;

    // Frame tiles
    base::Rect frame_up(72 * scale_, 0, 16 * scale_, 8 * scale_);
    base::Rect frame_down(72 * scale_, 24 * scale_, 16 * scale_, 8 * scale_);
    base::Rect frame_left(64 * scale_, 8 * scale_, 8 * scale_, 16 * scale_);
    base::Rect frame_right(88 * scale_, 8 * scale_, 8 * scale_, 16 * scale_);

    base::Vec2i frame_up_pos(draw_offset.x + 8 * scale_, draw_offset.y);
    base::Vec2i frame_down_pos(draw_offset.x + 8 * scale_,
                               draw_offset.y + bound_.height - 8 * scale_);
    base::Vec2i frame_left_pos(draw_offset.x, draw_offset.y + 8 * scale_);
    base::Vec2i frame_right_pos(draw_offset.x + bound_.width - 8 * scale_,
                                draw_offset.y + 8 * scale_);

    quad_index += BuildTilesAlongAxis(
        TileAxis::HORIZONTAL, frame_up, frame_up_pos, base::Vec4(opacity_norm),
        bound_.width - 16 * scale_, windowskin->size, &quads[quad_index]);
    quad_index += BuildTilesAlongAxis(TileAxis::HORIZONTAL, frame_down,
                                      frame_down_pos, base::Vec4(opacity_norm),
                                      bound_.width - 16 * scale_,
                                      windowskin->size, &quads[quad_index]);
    quad_index += BuildTilesAlongAxis(TileAxis::VERTICAL, frame_left,
                                      frame_left_pos, base::Vec4(opacity_norm),
                                      bound_.height - 16 * scale_,
                                      windowskin->size, &quads[quad_index]);
    quad_index += BuildTilesAlongAxis(TileAxis::VERTICAL, frame_right,
                                      frame_right_pos, base::Vec4(opacity_norm),
                                      bound_.height - 16 * scale_,
                                      windowskin->size, &quads[quad_index]);
  }

  // Make sure quads drawcall count
  agent_.background_draw_count = quad_index;

  // Update vertex buffer
  if (!agent_.background_cache.empty())
    agent_.background_batch.QueueWrite(**render_context,
                                       agent_.background_cache.data(),
                                       agent_.background_cache.size());
}

void WindowImpl::GPUCompositeControlLayerInternal(
    renderer::RenderContext* render_context,
    BitmapAgent* windowskin,
    BitmapAgent* contents) {
  const float contents_opacity_norm = contents_opacity_ / 255.0f;
  const float cursor_opacity_norm = cursor_opacity_ / 255.0f;

  // Cursor render
  auto build_cursor_internal = [&](const base::Rect& rect, int32_t unit,
                                   base::Rect quad_rects[9]) {
    const int32_t w = rect.width;
    const int32_t h = rect.height;
    const int32_t l = rect.x;
    const int32_t r = l + w;
    const int32_t t = rect.y;
    const int32_t b = t + h;

    int32_t i = 0;
    quad_rects[i++] = {{l, t}, unit};                // Left-Top
    quad_rects[i++] = {{r - unit, t}, unit};         // Right-Top
    quad_rects[i++] = {{r - unit, b - unit}, unit};  // Right-Bottom
    quad_rects[i++] = {{l, b - unit}, unit};         // Left-Bottom

    quad_rects[i++] = {l, t + unit, unit, h - unit * 2};         // Left
    quad_rects[i++] = {r - unit, t + unit, unit, h - unit * 2};  // Right
    quad_rects[i++] = {l + unit, t, w - unit * 2, unit};         // Top
    quad_rects[i++] = {l + unit, b - unit, w - unit * 2, unit};  // Bottom
    quad_rects[i++] = {l + unit, t + unit, w - unit * 2,
                       h - unit * 2};  // Center
  };

  auto build_cursor_quads = [&](const base::Rect& src, const base::Rect& dst,
                                renderer::Quad vert[9]) {
    int32_t quads_count = 0;

    base::Rect texcoords[9], positions[9];
    build_cursor_internal(src, scale_, texcoords);
    build_cursor_internal(dst, scale_, positions);

    const base::Vec4 draw_color(cursor_opacity_norm * contents_opacity_norm);

    for (int32_t i = 0; i < 9; ++i) {
      const auto interaction = base::MakeIntersect(positions[i], bound_);
      if (interaction.width && interaction.height) {
        renderer::Quad::SetPositionRect(&vert[i], interaction);
        renderer::Quad::SetTexCoordRect(&vert[i], texcoords[i],
                                        windowskin->size);
        renderer::Quad::SetColor(&vert[i], draw_color);
        quads_count++;
      }
    }

    return quads_count;
  };

  // Generate quads
  const base::Vec2i draw_offset = bound_.Position();
  auto& quads = agent_.controls_cache;
  quads.resize(9 + 4 + 1 + 1);
  int32_t quad_index = 0;

  if (windowskin) {
    // Cursor render (9)
    const auto cursor_rect = cursor_rect_->AsBaseRect();
    if (cursor_rect.width > 0 && cursor_rect.height > 0) {
      const base::Rect cursor_dest_rect(
          draw_offset.x + cursor_rect.x + 8 * scale_,
          draw_offset.y + cursor_rect.y + 8 * scale_, cursor_rect.width,
          cursor_rect.height);
      const base::Rect cursor_src(64 * scale_, 32 * scale_, 16 * scale_,
                                  16 * scale_);
      quad_index +=
          build_cursor_quads(cursor_src, cursor_dest_rect, &quads[quad_index]);
    }

    // Arrows render (0-4)
    const base::Vec2i scroll =
        draw_offset + ((bound_.Size() - base::Vec2i(8 * scale_)) / 2);
    base::Rect scroll_arrow_up_pos = base::Rect(
        scroll.x, draw_offset.y + 2 * scale_, 8 * scale_, 4 * scale_);
    base::Rect scroll_arrow_down_pos =
        base::Rect(scroll.x, draw_offset.y + bound_.height - 6 * scale_,
                   8 * scale_, 4 * scale_);
    base::Rect scroll_arrow_left_pos = base::Rect(
        draw_offset.x + 2 * scale_, scroll.y, 4 * scale_, 8 * scale_);
    base::Rect scroll_arrow_right_pos =
        base::Rect(draw_offset.x + bound_.width - 6 * scale_, scroll.y,
                   4 * scale_, 8 * scale_);

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
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_left_pos);
        renderer::Quad::SetTexCoordRect(
            &quads[quad_index], scroll_arrow_left_src, windowskin->size);
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if (origin_.y > 0) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_up_pos);
        renderer::Quad::SetTexCoordRect(&quads[quad_index], scroll_arrow_up_src,
                                        windowskin->size);
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if ((bound_.width - 16 * scale_) < (contents->size.x - origin_.x)) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_right_pos);
        renderer::Quad::SetTexCoordRect(
            &quads[quad_index], scroll_arrow_right_src, windowskin->size);
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if ((bound_.height - 16 * scale_) < (contents->size.y - origin_.y)) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_down_pos);
        renderer::Quad::SetTexCoordRect(
            &quads[quad_index], scroll_arrow_down_src, windowskin->size);
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }
    }

    // Pause render (0-1)
    base::Rect pause_animation[] = {
        {80 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
        {88 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
        {80 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
        {88 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
    };

    if (pause_) {
      renderer::Quad::SetPositionRect(
          &quads[quad_index],
          base::Rect(draw_offset.x + (bound_.width - 8 * scale_) / 2,
                     draw_offset.y + bound_.height - 8 * scale_, 8 * scale_,
                     8 * scale_));
      renderer::Quad::SetTexCoordRect(&quads[quad_index],
                                      pause_animation[pause_index_ / 8],
                                      windowskin->size);
      renderer::Quad::SetColor(&quads[quad_index],
                               base::Vec4(contents_opacity_norm));
      quad_index++;
    }

    // Make sure controls quads count
    agent_.controls_draw_count = quad_index;
  }

  // Contents (0-1)
  if (contents) {
    const base::Vec2i content_offset(draw_offset.x + 8 * scale_ + origin_.x,
                                     draw_offset.y + 8 * scale_ + origin_.y);
    const base::Rect content_bound(
        draw_offset.x + 8 * scale_, draw_offset.y + 8 * scale_,
        bound_.width - 16 * scale_, bound_.height - 16 * scale_);

    const auto interaction = base::MakeIntersect(
        base::Rect(content_offset, contents->size), content_bound);

    renderer::Quad::SetPositionRect(&quads[quad_index], interaction);
    renderer::Quad::SetTexCoordRectNorm(&quads[quad_index], base::Rect(0, 1));
    renderer::Quad::SetColor(&quads[quad_index],
                             base::Vec4(contents_opacity_norm));

    // Make sure contents quad offset
    agent_.contents_quad_offset = quad_index;
  }

  // Make sure index buffer count
  context()->render_device->GetQuadIndex()->Allocate(quad_index);

  // Update vertex buffer
  if (!quads.empty())
    agent_.controls_batch.QueueWrite(**render_context, quads.data(),
                                     quads.size());
}

void WindowImpl::GPURenderBackgroundLayerInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    BitmapAgent* windowskin) {
  if (windowskin) {
    auto& pipeline_set = context()->render_device->GetPipelines()->base;
    auto* pipeline =
        pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

    // Setup shader resource
    agent_.base_binding.u_transform->Set(world_binding);
    agent_.base_binding.u_texture->Set(windowskin->resource);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.background_batch;
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
            *agent_.base_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.background_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    (*render_context)->DrawIndexed(draw_indexed_attribs);
  }
}

void WindowImpl::GPURenderControlLayerInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    BitmapAgent* windowskin,
    BitmapAgent* contents) {
  if (windowskin) {
    auto& pipeline_set = context()->render_device->GetPipelines()->base;
    auto* pipeline =
        pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

    // Setup shader resource
    agent_.base_binding.u_transform->Set(world_binding);
    agent_.base_binding.u_texture->Set(windowskin->resource);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.controls_batch;
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
            *agent_.base_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.controls_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    (*render_context)->DrawIndexed(draw_indexed_attribs);
  }

  if (contents) {
    auto& pipeline_set = context()->render_device->GetPipelines()->base;
    auto* pipeline =
        pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

    // Setup shader resource
    agent_.content_binding.u_transform->Set(world_binding);
    agent_.content_binding.u_texture->Set(contents->resource);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.controls_batch;
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
            *agent_.content_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = agent_.contents_quad_offset * 6;
    (*render_context)->DrawIndexed(draw_indexed_attribs);
  }
}

}  // namespace content
