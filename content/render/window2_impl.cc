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

  tone_->AddObserver(base::BindRepeating(
      &Window2Impl::BackgroundTextureObserverInternal, base::Unretained(this)));

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
  background_dirty_ = true;
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
  if (windowskin_)
    windowskin_->AddCanvasObserver(
        base::BindRepeating(&Window2Impl::BackgroundTextureObserverInternal,
                            base::Unretained(this)));
  background_dirty_ = true;
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
  background_dirty_ = true;
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
  background_dirty_ = true;
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
  background_dirty_ = true;
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
  background_dirty_ = true;
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
    background_dirty_ = false;
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPURenderWindowQuadsInternal(params->context, params->world_binding,
                                 windowskin_agent, contents_agent, padding_rect,
                                 params->viewport, params->origin);
  }
}

void Window2Impl::BackgroundTextureObserverInternal() {
  background_dirty_ = true;
}

void Window2Impl::GPUCreateWindowInternal() {
  agent_.background_batch =
      renderer::QuadBatch::Make(**context()->render_device);
  agent_.controls_batch = renderer::QuadBatch::Make(**context()->render_device);
  agent_.shader_binding =
      context()->render_device->GetPipelines()->base.CreateBinding();

  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::Binding_Flat::Params),
      "window2.uniform", &agent_.uniform_buffer, Diligent::USAGE_DEFAULT);
}

void Window2Impl::GPUCompositeWindowQuadsInternal(
    renderer::RenderContext* render_context,
    BitmapAgent* contents,
    BitmapAgent* windowskin,
    const base::Rect& padding_rect) {
  // Create texture if need
  if (windowskin) {
    if (!agent_.background_texture ||
        static_cast<int32_t>(agent_.background_texture->GetDesc().Width) !=
            bound_.width ||
        static_cast<int32_t>(agent_.background_texture->GetDesc().Height) !=
            bound_.height) {
      agent_.background_texture.Release();
      renderer::CreateTexture2D(
          **context()->render_device, &agent_.background_texture,
          "window2.background", bound_.Size(), Diligent::USAGE_DEFAULT,
          Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);

      renderer::WorldTransform world_matrix;
      renderer::MakeProjectionMatrix(world_matrix.projection, bound_.Size());
      renderer::MakeIdentityMatrix(world_matrix.transform);

      agent_.background_world.Release();
      Diligent::CreateUniformBuffer(
          **context()->render_device, sizeof(world_matrix), "window2.world",
          &agent_.background_world, Diligent::USAGE_IMMUTABLE,
          Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_NONE,
          &world_matrix);

      background_dirty_ = true;
    }

    // Render background texture if need
    if (background_dirty_) {
      background_dirty_ = false;
      int32_t quad_count = 0;

      // Background part
      {
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
      }

      base::Vector<renderer::Quad> quads;
      quads.resize(quad_count);
      renderer::Quad* quad_ptr = quads.data();

      // Generate quads
      const float back_opacity_norm = back_opacity_ / 255.0f;
      int32_t background_draw_count = 0;

      // Background part
      {
        // Stretch layer + Tile layer
        const base::Rect background1_src(0, 0, 32 * scale_, 32 * scale_);
        const base::Rect background2_src(0, 32 * scale_, 32 * scale_,
                                         32 * scale_);
        const base::Rect background_dest(scale_, scale_,
                                         bound_.width - 2 * scale_,
                                         bound_.height - 2 * scale_);
        renderer::Quad::SetPositionRect(quad_ptr, background_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, background1_src,
                                        windowskin->size);
        renderer::Quad::SetColor(quad_ptr, base::Vec4(back_opacity_norm));
        quad_ptr++;

        int32_t tiles_count = BuildTiles(background2_src, background_dest,
                                         base::Vec4(back_opacity_norm),
                                         windowskin->size, quad_ptr);
        background_draw_count += tiles_count;
        quad_ptr += tiles_count;

        // Corners
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

        renderer::Quad::SetPositionRect(quad_ptr, corner_left_top_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_top_src,
                                        windowskin->size);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_right_top_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_top_src,
                                        windowskin->size);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_left_bottom_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_bottom_src,
                                        windowskin->size);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_right_bottom_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_bottom_src,
                                        windowskin->size);
        quad_ptr++;

        // Tiles frame
        const base::Rect frame_up_src(40 * scale_, 0, 16 * scale_, 8 * scale_);
        const base::Rect frame_down_src(40 * scale_, 24 * scale_, 16 * scale_,
                                        8 * scale_);
        const base::Rect frame_left_src(32 * scale_, 8 * scale_, 8 * scale_,
                                        16 * scale_);
        const base::Rect frame_right_src(56 * scale_, 8 * scale_, 8 * scale_,
                                         16 * scale_);

        quad_ptr += BuildTilesAlongAxis(
            TileAxis::HORIZONTAL, frame_up_src, base::Vec2i(8 * scale_, 0),
            base::Vec4(1.0f), bound_.width - 16 * scale_, windowskin->size,
            quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::HORIZONTAL, frame_down_src,
            base::Vec2i(8 * scale_, bound_.height - 8 * scale_),
            base::Vec4(1.0f), bound_.width - 16 * scale_, windowskin->size,
            quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::VERTICAL, frame_left_src, base::Vec2i(0, 8 * scale_),
            base::Vec4(1.0f), bound_.height - 16 * scale_, windowskin->size,
            quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::VERTICAL, frame_right_src,
            base::Vec2i(bound_.width - 8 * scale_, 8 * scale_),
            base::Vec4(1.0f), bound_.height - 16 * scale_, windowskin->size,
            quad_ptr);
      }

      agent_.background_batch.QueueWrite(**render_context, quads.data(),
                                         quads.size());

      renderer::Binding_Flat::Params uniform;
      uniform.Color = base::Vec4();
      uniform.Tone = tone_->AsNormColor();

      (*render_context)
          ->UpdateBuffer(agent_.uniform_buffer, 0, sizeof(uniform), &uniform,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      {
        auto& pipeline_set_tone =
            context()->render_device->GetPipelines()->viewport;
        auto* pipeline_tone =
            pipeline_set_tone.GetPipeline(renderer::BLEND_TYPE_NORMAL, false);
        auto* pipeline_tone_alpha = pipeline_set_tone.GetPipeline(
            renderer::BLEND_TYPE_KEEP_ALPHA, false);
        auto& pipeline_set_base =
            context()->render_device->GetPipelines()->base;
        auto* pipeline_base =
            pipeline_set_base.GetPipeline(renderer::BLEND_TYPE_NORMAL, false);

        float clear_color[] = {0, 0, 0, 0};
        auto* render_target_view = agent_.background_texture->GetDefaultView(
            Diligent::TEXTURE_VIEW_RENDER_TARGET);
        (*render_context)
            ->SetRenderTargets(
                1, &render_target_view, nullptr,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        (*render_context)
            ->ClearRenderTarget(
                render_target_view, clear_color,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        render_context->ScissorState()->Apply(bound_.Size());
        context()->render_device->GetQuadIndex()->Allocate(quads.size() + 20);

        // Create binding
        renderer::Binding_Flat flat_binding =
            context()->render_device->GetPipelines()->viewport.CreateBinding();
        renderer::Binding_Base base_binding =
            context()->render_device->GetPipelines()->base.CreateBinding();

        // Setup uniform params
        flat_binding.u_transform->Set(agent_.background_world);
        flat_binding.u_texture->Set(windowskin->resource);
        flat_binding.u_params->Set(agent_.uniform_buffer);

        base_binding.u_transform->Set(agent_.background_world);
        base_binding.u_texture->Set(windowskin->resource);

        // Apply vertex index
        Diligent::IBuffer* const vertex_buffer = *agent_.background_batch;
        (*render_context)
            ->SetVertexBuffers(
                0, 1, &vertex_buffer, nullptr,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        (*render_context)
            ->SetIndexBuffer(
                **context()->render_device->GetQuadIndex(), 0,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Apply pipeline state
        (*render_context)->SetPipelineState(pipeline_tone);
        (*render_context)
            ->CommitShaderResources(
                *flat_binding,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Stretch Layer
        {
          Diligent::DrawIndexedAttribs draw_indexed_attribs;
          draw_indexed_attribs.NumIndices = 6;
          draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
          (*render_context)->DrawIndexed(draw_indexed_attribs);
        }

        // Apply pipeline state
        (*render_context)->SetPipelineState(pipeline_tone_alpha);
        (*render_context)
            ->CommitShaderResources(
                *flat_binding,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Tiled Layer
        {
          Diligent::DrawIndexedAttribs draw_indexed_attribs;
          draw_indexed_attribs.NumIndices = background_draw_count * 6;
          draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
          draw_indexed_attribs.FirstIndexLocation = 6;
          (*render_context)->DrawIndexed(draw_indexed_attribs);
        }

        // Apply pipeline state
        (*render_context)->SetPipelineState(pipeline_base);
        (*render_context)
            ->CommitShaderResources(
                *base_binding,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Frames and Corners
        {
          background_draw_count++;
          Diligent::DrawIndexedAttribs draw_indexed_attribs;
          draw_indexed_attribs.NumIndices =
              (quads.size() - background_draw_count) * 6;
          draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
          draw_indexed_attribs.FirstIndexLocation = background_draw_count * 6;
          (*render_context)->DrawIndexed(draw_indexed_attribs);
        }
      }
    }
  }

  // Reset draw count
  agent_.background_draw_count = 0;
  agent_.controls_draw_count = 0;
  agent_.cursor_draw_count = 0;
  agent_.contents_draw_count = 0;

  // Render background and controls
  {
    base::Vector<renderer::Quad> quads;
    // Background + Pause Anime + Arrows + Cursor + Contents
    quads.resize(1 + 1 + 4 + 9 + 1);

    renderer::Quad* quad_ptr = quads.data();
    const base::Vec2i offset = bound_.Position();
    const float openness_norm = openness_ / 255.0f;
    const float contents_opacity_norm = contents_opacity_ / 255.0f;
    const float cursor_opacity_norm = cursor_opacity_ / 255.0f;

    // Background texture quad
    if (windowskin) {
      const base::Rect background_dest(
          offset.x,
          static_cast<float>(offset.y) +
              static_cast<float>(bound_.height / 2.0f) * (1.0f - openness_norm),
          bound_.width, bound_.height * openness_norm);
      renderer::Quad::SetPositionRect(quad_ptr, background_dest);
      renderer::Quad::SetTexCoordRect(quad_ptr, base::Rect(bound_.Size()),
                                      bound_.Size());
      renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity_ / 255.0f));
      agent_.background_draw_count += 1;
      quad_ptr += 1;
    }

    // Controls & Contents
    if (openness_ >= 255) {
      // Controls offset
      base::Vec2i content_offset = offset + padding_rect.Position();

      if (windowskin) {
        const base::Vec2i arrow_display_offset =
            (bound_.Size() - base::Vec2i(8 * scale_)) / 2;

        if (arrows_visible_) {
          // Arrows
          const base::Rect arrow_up_dest(offset.x + arrow_display_offset.x,
                                         offset.y + 2 * scale_, 8 * scale_,
                                         4 * scale_);
          const base::Rect arrow_down_dest(
              offset.x + arrow_display_offset.x,
              offset.y + bound_.height - 6 * scale_, 8 * scale_, 4 * scale_);
          const base::Rect arrow_left_dest(offset.x + 2 * scale_,
                                           offset.y + arrow_display_offset.y,
                                           4 * scale_, 8 * scale_);
          const base::Rect arrow_right_dest(
              offset.x + bound_.width - 6 * scale_,
              offset.y + arrow_display_offset.y, 4 * scale_, 8 * scale_);

          const base::Rect arrow_up_src(44 * scale_, 8 * scale_, 8 * scale_,
                                        4 * scale_);
          const base::Rect arrow_down_src(44 * scale_, 20 * scale_, 8 * scale_,
                                          4 * scale_);
          const base::Rect arrow_left_src(40 * scale_, 12 * scale_, 4 * scale_,
                                          8 * scale_);
          const base::Rect arrow_right_src(52 * scale_, 12 * scale_, 4 * scale_,
                                           8 * scale_);

          if (contents && arrows_visible_) {
            if (origin_.x > 0) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_left_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_left_src,
                                              windowskin->size);
              renderer::Quad::SetColor(quad_ptr,
                                       base::Vec4(contents_opacity_norm));
              agent_.controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (origin_.y > 0) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_up_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_up_src,
                                              windowskin->size);
              renderer::Quad::SetColor(quad_ptr,
                                       base::Vec4(contents_opacity_norm));
              agent_.controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (padding_rect.width < (contents->size.x - origin_.x)) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_right_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_right_src,
                                              windowskin->size);
              renderer::Quad::SetColor(quad_ptr,
                                       base::Vec4(contents_opacity_norm));
              agent_.controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (padding_rect.height < (contents->size.y - origin_.y)) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_down_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_down_src,
                                              windowskin->size);
              renderer::Quad::SetColor(quad_ptr,
                                       base::Vec4(contents_opacity_norm));
              agent_.controls_draw_count += 1;
              quad_ptr += 1;
            }
          }
        }

        // Cursor
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
            quad_rects[i++] =
                base::Rect(x1, y1 + scale_, scale_, h - scale_ * 2);
            // Right
            quad_rects[i++] =
                base::Rect(x2 - scale_, y1 + scale_, scale_, h - scale_ * 2);
            // Top
            quad_rects[i++] =
                base::Rect(x1 + scale_, y1, w - scale_ * 2, scale_);
            // Bottom
            quad_rects[i++] =
                base::Rect(x1 + scale_, y2 - scale_, w - scale_ * 2, scale_);
            // Center
            quad_rects[i++] = base::Rect(x1 + scale_, y1 + scale_,
                                         w - scale_ * 2, h - scale_ * 2);
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

            int32_t count =
                build_cursor_quads(cursor_src, cursor_dest, quad_ptr);
            agent_.cursor_draw_count += count;
            quad_ptr += count;
          }
        }

        // Pause
        const base::Rect pause_src[] = {
            {48 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
            {56 * scale_, 32 * scale_, 8 * scale_, 8 * scale_},
            {48 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
            {56 * scale_, 40 * scale_, 8 * scale_, 8 * scale_},
        };

        if (pause_) {
          const base::Rect pause_dest(offset.x + arrow_display_offset.x,
                                      offset.y + bound_.height - 8 * scale_,
                                      8 * scale_, 8 * scale_);
          renderer::Quad::SetPositionRect(quad_ptr, pause_dest);
          renderer::Quad::SetTexCoordRect(quad_ptr, pause_src[pause_index_ / 8],
                                          windowskin->size);
          renderer::Quad::SetColor(quad_ptr, base::Vec4(contents_opacity_norm));
          agent_.controls_draw_count += 1;
          quad_ptr += 1;
        }
      }

      // Contents
      if (contents) {
        renderer::Quad::SetPositionRect(
            quad_ptr, base::Rect(content_offset - origin_, contents->size));
        renderer::Quad::SetTexCoordRect(quad_ptr, base::Rect(contents->size),
                                        contents->size);
        renderer::Quad::SetColor(quad_ptr, base::Vec4(contents_opacity_norm));
        agent_.contents_draw_count += 1;
      }
    }

    // Uplopad data
    agent_.controls_batch.QueueWrite(**render_context, quads.data(),
                                     quads.size());
  }
}

void Window2Impl::GPURenderWindowQuadsInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    BitmapAgent* windowskin,
    BitmapAgent* contents,
    const base::Rect& padding_rect,
    const base::Rect& last_viewport,
    const base::Vec2i& last_origin) {
  auto& pipeline_set = context()->render_device->GetPipelines()->base;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

  // Setup world uniform
  agent_.shader_binding.u_transform->Set(world_binding);

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

  int32_t count = 0;
  if (agent_.background_draw_count > 0) {
    // Setup texture
    agent_.shader_binding.u_texture->Set(
        agent_.background_texture->GetDefaultView(
            Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
    (*render_context)
        ->CommitShaderResources(
            *agent_.shader_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.background_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = count * 6;
    (*render_context)->DrawIndexed(draw_indexed_attribs);

    count += agent_.background_draw_count;
  }

  if (agent_.controls_draw_count > 0) {
    // Setup texture
    agent_.shader_binding.u_texture->Set(windowskin->resource);
    (*render_context)
        ->CommitShaderResources(
            *agent_.shader_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.controls_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = count * 6;
    (*render_context)->DrawIndexed(draw_indexed_attribs);

    count += agent_.controls_draw_count;
  }

  const base::Rect contents_region(
      last_viewport.x + bound_.x + padding_rect.x - last_origin.x,
      last_viewport.y + bound_.y + padding_rect.y - last_origin.y,
      padding_rect.width, padding_rect.height);
  auto scissor_region = base::MakeIntersect(last_viewport, contents_region);
  if (!scissor_region.width || !scissor_region.height)
    return;

  render_context->ScissorState()->Push(scissor_region);

  if (agent_.cursor_draw_count > 0) {
    // Setup texture
    agent_.shader_binding.u_texture->Set(windowskin->resource);
    (*render_context)
        ->CommitShaderResources(
            *agent_.shader_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.cursor_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = count * 6;
    (*render_context)->DrawIndexed(draw_indexed_attribs);

    count += agent_.cursor_draw_count;
  }

  if (agent_.contents_draw_count > 0) {
    // Setup texture
    agent_.shader_binding.u_texture->Set(contents->resource);
    (*render_context)
        ->CommitShaderResources(
            *agent_.shader_binding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = agent_.contents_draw_count * 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = count * 6;
    (*render_context)->DrawIndexed(draw_indexed_attribs);
  }

  render_context->ScissorState()->Pop();
}

}  // namespace content
