// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window_impl.h"

#include "content/context/execution_context.h"
#include "content/render/tilequad.h"
#include "renderer/utils/texture_utils.h"

namespace content {

// static
scoped_refptr<Window> Window::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  int32_t scale_,
                                  ExceptionState& exception_state) {
  return base::MakeRefCounted<WindowImpl>(
      execution_context, ViewportImpl::From(viewport), std::max(1, scale_));
}

///////////////////////////////////////////////////////////////////////////////
// WindowImpl Implement

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

DISPOSABLE_DEFINITION(WindowImpl);

void WindowImpl::Update(ExceptionState& exception_state) {
  DISPOSE_CHECK;

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

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Viewport,
    scoped_refptr<Viewport>,
    WindowImpl,
    { return viewport_; },
    {
      DISPOSE_CHECK;
      if (viewport_ == value)
        return;
      viewport_ = ViewportImpl::From(value);
      DrawNodeController* controller = viewport_
                                           ? viewport_->GetDrawableController()
                                           : context()->screen_drawable_node;
      background_node_.RebindController(controller);
      control_node_.RebindController(controller);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Windowskin,
    scoped_refptr<Bitmap>,
    WindowImpl,
    { return windowskin_; },
    {
      DISPOSE_CHECK;
      windowskin_ = CanvasImpl::FromBitmap(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Contents,
    scoped_refptr<Bitmap>,
    WindowImpl,
    { return contents_; },
    {
      DISPOSE_CHECK;
      contents_ = CanvasImpl::FromBitmap(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Stretch,
    bool,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return stretch_;
    },
    {
      DISPOSE_CHECK;
      stretch_ = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    CursorRect,
    scoped_refptr<Rect>,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(nullptr);
      return cursor_rect_;
    },
    {
      DISPOSE_CHECK;
      CHECK_ATTRIBUTE_VALUE;
      *cursor_rect_ = *RectImpl::From(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Active,
    bool,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return active_;
    },
    {
      DISPOSE_CHECK;
      active_ = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Visible,
    bool,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return background_node_.GetVisibility();
    },
    {
      DISPOSE_CHECK;
      background_node_.SetNodeVisibility(value);
      control_node_.SetNodeVisibility(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Pause,
    bool,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return pause_;
    },
    {
      DISPOSE_CHECK;
      pause_ = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    X,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bound_.x;
    },
    {
      DISPOSE_CHECK;
      bound_.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Y,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bound_.y;
    },
    {
      DISPOSE_CHECK;
      bound_.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Width,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bound_.width;
    },
    {
      DISPOSE_CHECK;
      bound_.width = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Height,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bound_.height;
    },
    {
      DISPOSE_CHECK;
      bound_.height = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Z,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return background_node_.GetSortKeys()->weight[0];
    },
    {
      DISPOSE_CHECK;
      background_node_.SetNodeSortWeight(value);
      control_node_.SetNodeSortWeight(value + 2);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Ox,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return origin_.x;
    },
    {
      DISPOSE_CHECK;
      origin_.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Oy,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return origin_.y;
    },
    {
      DISPOSE_CHECK;
      origin_.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Opacity,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return opacity_;
    },
    {
      DISPOSE_CHECK;
      opacity_ = std::clamp(value, 0, 255);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    BackOpacity,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return back_opacity_;
    },
    {
      DISPOSE_CHECK;
      back_opacity_ = std::clamp(value, 0, 255);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    ContentsOpacity,
    int32_t,
    WindowImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return contents_opacity_;
    },
    {
      DISPOSE_CHECK;
      contents_opacity_ = std::clamp(value, 0, 255);
    });

void WindowImpl::OnObjectDisposed() {
  background_node_.DisposeNode();
  control_node_.DisposeNode();

  GPUData empty_data;
  std::swap(gpu_, empty_data);
}

void WindowImpl::BackgroundNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  BitmapTexture* windowskin_agent =
      Disposable::IsValid(windowskin_.get()) ? **windowskin_ : nullptr;

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

  BitmapTexture* windowskin_agent =
      Disposable::IsValid(windowskin_.get()) ? **windowskin_ : nullptr;
  BitmapTexture* contents_agent =
      Disposable::IsValid(contents_.get()) ? **contents_ : nullptr;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUCompositeControlLayerInternal(params->context, windowskin_agent,
                                     contents_agent);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderControlLayerInternal(params->context, params->screen_depth_stencil,
                                  params->world_binding, windowskin_agent,
                                  contents_agent);
  }
}

void WindowImpl::GPUCreateWindowInternal() {
  gpu_.background_batch = renderer::QuadBatch::Make(**context()->render_device);
  gpu_.controls_batch = renderer::QuadBatch::Make(**context()->render_device);

  gpu_.color_binding = context()->render.pipeline_loader->color.CreateBinding();
  gpu_.base_binding = context()->render.pipeline_loader->base.CreateBinding();
  gpu_.content_binding =
      context()->render.pipeline_loader->base.CreateBinding();
}

void WindowImpl::GPUCompositeBackgroundLayerInternal(
    Diligent::IDeviceContext* render_context,
    BitmapTexture* windowskin) {
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
    auto& quads = gpu_.background_cache;
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
                                      windowskin->size.Recast<float>());
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

    renderer::Quad::SetPositionRect(
        &quads[quad_index], base::Rect{draw_offset, base::Vec2i(8 * scale_)});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_top,
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x + bound_.width - 8 * scale_, draw_offset.y},
                   base::Vec2i(8 * scale_)});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_top,
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x + bound_.width - 8 * scale_,
                    draw_offset.y + bound_.height - 8 * scale_},
                   base::Vec2i(8 * scale_)});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_right_bottom,
                                    windowskin->size.Recast<float>());
    renderer::Quad::SetColor(&quads[quad_index], base::Vec4(opacity_norm));
    quad_index++;
    renderer::Quad::SetPositionRect(
        &quads[quad_index],
        base::Rect{{draw_offset.x, draw_offset.y + bound_.height - 8 * scale_},
                   base::Vec2i(8 * scale_)});
    renderer::Quad::SetTexCoordRect(&quads[quad_index], corner_left_bottom,
                                    windowskin->size.Recast<float>());
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
  gpu_.background_draw_count = quad_index;

  // Update vertex buffer
  if (!gpu_.background_cache.empty())
    gpu_.background_batch.QueueWrite(render_context,
                                     gpu_.background_cache.data(),
                                     gpu_.background_cache.size());
}

void WindowImpl::GPUCompositeControlLayerInternal(
    Diligent::IDeviceContext* render_context,
    BitmapTexture* windowskin,
    BitmapTexture* contents) {
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

  auto build_cursor_quads = [&](const base::Rect& src, const base::Rect& dst,
                                renderer::Quad vert[9]) {
    base::Rect texcoords[9], positions[9];
    const base::Vec4 draw_color(cursor_opacity_norm * contents_opacity_norm);
    build_cursor_internal(src, scale_, texcoords);
    build_cursor_internal(dst, scale_, positions);

    for (int32_t i = 0; i < 9; ++i) {
      renderer::Quad::SetPositionRect(&vert[i], positions[i]);
      renderer::Quad::SetTexCoordRect(&vert[i], texcoords[i],
                                      windowskin->size.Recast<float>());
      renderer::Quad::SetColor(&vert[i], draw_color);
    }

    return 9;
  };

  // Generate quads
  const base::Vec2i draw_offset = bound_.Position();
  auto& quads = gpu_.controls_cache;
  quads.resize(9 + 4 + 1 + 1 + 1);
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
        renderer::Quad::SetTexCoordRect(&quads[quad_index],
                                        scroll_arrow_left_src,
                                        windowskin->size.Recast<float>());
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if (origin_.y > 0) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_up_pos);
        renderer::Quad::SetTexCoordRect(&quads[quad_index], scroll_arrow_up_src,
                                        windowskin->size.Recast<float>());
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if ((bound_.width - 16 * scale_) < (contents->size.x - origin_.x)) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_right_pos);
        renderer::Quad::SetTexCoordRect(&quads[quad_index],
                                        scroll_arrow_right_src,
                                        windowskin->size.Recast<float>());
        renderer::Quad::SetColor(&quads[quad_index],
                                 base::Vec4(contents_opacity_norm));
        quad_index++;
      }

      if ((bound_.height - 16 * scale_) < (contents->size.y - origin_.y)) {
        renderer::Quad::SetPositionRect(&quads[quad_index],
                                        scroll_arrow_down_pos);
        renderer::Quad::SetTexCoordRect(&quads[quad_index],
                                        scroll_arrow_down_src,
                                        windowskin->size.Recast<float>());
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
                                      windowskin->size.Recast<float>());
      renderer::Quad::SetColor(&quads[quad_index],
                               base::Vec4(contents_opacity_norm));
      quad_index++;
    }

    // Make sure controls quads count
    gpu_.controls_draw_count = quad_index;
  }

  // Contents (0-1)
  if (contents) {
    const base::Vec2i content_offset(draw_offset.x + 8 * scale_ - origin_.x,
                                     draw_offset.y + 8 * scale_ - origin_.y);

    renderer::Quad::SetPositionRect(&quads[quad_index],
                                    base::Rect(content_offset, contents->size));
    renderer::Quad::SetTexCoordRectNorm(
        &quads[quad_index], base::RectF(base::Vec2(0), base::Vec2(1)));
    renderer::Quad::SetColor(&quads[quad_index],
                             base::Vec4(contents_opacity_norm));

    // Make sure contents quad offset
    gpu_.contents_quad_offset = quad_index;
    quad_index++;
  }

  // Window stencil mask quad (1)
  const base::Rect stencil_mask_bound(
      bound_.x + 8 * scale_, bound_.y + 8 * scale_, bound_.width - 16 * scale_,
      bound_.height - 16 * scale_);
  renderer::Quad::SetPositionRect(&quads[quad_index], stencil_mask_bound);
  gpu_.mask_quad_offset = quad_index;
  quad_index++;

  // Make sure index buffer count
  context()->render.quad_index->Allocate(quad_index);

  // Update vertex buffer
  gpu_.controls_batch.QueueWrite(render_context, quads.data(), quads.size());
}

void WindowImpl::GPURenderBackgroundLayerInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::IBuffer* world_binding,
    BitmapTexture* windowskin) {
  if (windowskin) {
    auto* pipeline = context()->render.pipeline_states->window.RawPtr();

    // Setup shader resource
    gpu_.base_binding.u_transform->Set(world_binding);
    gpu_.base_binding.u_texture->Set(windowskin->shader_resource_view);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *gpu_.background_batch;
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render.quad_index, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply pipeline state
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *gpu_.base_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = gpu_.background_draw_count * 6;
    draw_indexed_attribs.IndexType =
        context()->render.quad_index->GetIndexType();
    render_context->DrawIndexed(draw_indexed_attribs);
  }
}

void WindowImpl::GPURenderControlLayerInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ITexture* depth_stencil,
    Diligent::IBuffer* world_binding,
    BitmapTexture* windowskin,
    BitmapTexture* contents) {
  const auto current_viewport =
      background_node_.GetParentViewport()->bound.Position() -
      background_node_.GetParentViewport()->origin;

  if (windowskin) {
    const base::Rect clipping_region(current_viewport.x + bound_.x,
                                     current_viewport.y + bound_.y,
                                     bound_.width, bound_.height);

    if (clipping_region()) {
      auto* pipeline = context()->render.pipeline_states->window.RawPtr();

      // Setup shader resource
      gpu_.base_binding.u_transform->Set(world_binding);
      gpu_.base_binding.u_texture->Set(windowskin->shader_resource_view);

      // Apply vertex index
      Diligent::IBuffer* const vertex_buffer = *gpu_.controls_batch;
      render_context->SetVertexBuffers(
          0, 1, &vertex_buffer, nullptr,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      render_context->SetIndexBuffer(
          **context()->render.quad_index, 0,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Apply pipeline state
      render_context->SetPipelineState(pipeline);
      render_context->CommitShaderResources(
          *gpu_.base_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = gpu_.controls_draw_count * 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      render_context->DrawIndexed(draw_indexed_attribs);
    }
  }

  if (contents) {
    const base::Rect clipping_region(current_viewport.x + bound_.x + 8 * scale_,
                                     current_viewport.y + bound_.y + 8 * scale_,
                                     bound_.width - 16 * scale_,
                                     bound_.height - 16 * scale_);

    if (clipping_region()) {
      auto* pipeline =
          context()->render.pipeline_states->color_write_stencil.RawPtr();

      // Clear stencil buffer
      render_context->ClearDepthStencil(
          depth_stencil->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL),
          Diligent::CLEAR_STENCIL_FLAG, 0.0f, 0,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Shader resource
      gpu_.color_binding.u_transform->Set(world_binding);

      render_context->SetPipelineState(pipeline);
      render_context->CommitShaderResources(
          *gpu_.color_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Set stencil ref value
      render_context->SetStencilRef(0xFF);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = gpu_.mask_quad_offset * 6;
      render_context->DrawIndexed(draw_indexed_attribs);
    } else {
      return;
    }

    {
      auto* pipeline =
          context()->render.pipeline_states->window_with_stencil.RawPtr();

      // Set stencil value
      render_context->SetStencilRef(0xFF);

      // Setup shader resource
      gpu_.content_binding.u_transform->Set(world_binding);
      gpu_.content_binding.u_texture->Set(contents->shader_resource_view);

      // Apply vertex index
      Diligent::IBuffer* const vertex_buffer = *gpu_.controls_batch;
      render_context->SetVertexBuffers(
          0, 1, &vertex_buffer, nullptr,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      render_context->SetIndexBuffer(
          **context()->render.quad_index, 0,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Apply pipeline state
      render_context->SetPipelineState(pipeline);
      render_context->CommitShaderResources(
          *gpu_.content_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      // Execute render command
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = gpu_.contents_quad_offset * 6;
      render_context->DrawIndexed(draw_indexed_attribs);
    }
  }
}

}  // namespace content
