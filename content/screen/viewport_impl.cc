// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/viewport_impl.h"

#include "content/canvas/canvas_impl.h"
#include "content/canvas/canvas_scheduler.h"
#include "content/common/rect_impl.h"
#include "renderer/utils/texture_utils.h"

namespace content {

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(execution_context, nullptr,
                                            execution_context->resolution);
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(execution_context, nullptr,
                                            RectImpl::From(rect)->AsBaseRect());
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(execution_context, nullptr,
                                            base::Rect(x, y, width, height));
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Viewport> parent,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  scoped_refptr<RectImpl> region = RectImpl::From(rect);
  return base::MakeRefCounted<ViewportImpl>(
      execution_context, ViewportImpl::From(parent), region->AsBaseRect());
}

ViewportImpl::ViewportImpl(ExecutionContext* execution_context,
                           scoped_refptr<ViewportImpl> parent,
                           const base::Rect& region)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()),
      rect_(base::MakeRefCounted<RectImpl>(region)),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &ViewportImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  GPUCreateViewportAgent();
}

ViewportImpl::~ViewportImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

scoped_refptr<ViewportImpl> ViewportImpl::From(scoped_refptr<Viewport> host) {
  return static_cast<ViewportImpl*>(host.get());
}

void ViewportImpl::SetLabel(const base::String& label,
                            ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

void ViewportImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool ViewportImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void ViewportImpl::Flash(scoped_refptr<Color> color,
                         uint32_t duration,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  std::optional<base::Vec4> flash_color = std::nullopt;
  if (color)
    flash_color = ColorImpl::From(color)->AsNormColor();
  flash_emitter_.Setup(flash_color, duration);
}

void ViewportImpl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  flash_emitter_.Update();
}

void ViewportImpl::Render(scoped_refptr<Bitmap> target,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  scoped_refptr<CanvasImpl> render_target = CanvasImpl::FromBitmap(target);
  BitmapAgent* bitmap_agent =
      render_target ? render_target->GetAgent() : nullptr;
  if (!bitmap_agent)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid render target.");

  // Check flash status
  if (flash_emitter_.IsFlashing() && flash_emitter_.IsInvalid())
    return;

  // Check viewport visible
  const base::Rect viewport_rect =
      base::MakeIntersect(bitmap_agent->size, rect_->AsBaseRect().Size());
  const base::Vec2i offset = -origin_;
  if (viewport_rect.width <= 0 || viewport_rect.height <= 0)
    return;

  // Submit pending canvas commands
  context()->canvas_scheduler->SubmitPendingPaintCommands();

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.context = context()->primary_render_context;
  controller_params.screen_buffer = bitmap_agent->data;
  controller_params.screen_depth_stencil = bitmap_agent->depth_stencil;
  controller_params.screen_size = bitmap_agent->size;
  controller_params.viewport = viewport_rect;
  controller_params.origin = origin_;

  // 0) Update uniform buffer if viewport region changed
  base::Rect transform_cache_rect(offset, controller_params.screen_size);
  if (!(transform_cache_ == transform_cache_rect)) {
    transform_cache_ = transform_cache_rect;
    GPUUpdateViewportTransform(controller_params.context, transform_cache_rect);
  }

  // 0.5) Reset intermediate layer if need
  if (!(effect_layer_cache_ == viewport_rect.Size())) {
    effect_layer_cache_ = viewport_rect.Size();
    GPUResetIntermediateLayer(viewport_rect.Size());
  }

  // 1) Execute pre-composite handler
  controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 1.5) Update sprite batch data
  context()->sprite_batcher->SubmitBatchDataAndResetCache(
      controller_params.context);

  // 2) Setup renderpass
  GPUFrameBeginRenderPassInternal(controller_params.context, bitmap_agent,
                                  offset, viewport_rect);

  // 3) Notify render a frame
  controller_params.root_world = bitmap_agent->world_buffer;
  controller_params.world_binding = agent_.world_uniform;
  controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  base::Vec4 composite_color = color_->AsNormColor();
  base::Vec4 flash_color = flash_emitter_.GetColor();
  base::Vec4 target_color = composite_color;
  if (flash_emitter_.IsFlashing())
    target_color =
        (flash_color.w > composite_color.w ? flash_color : composite_color);

  GPUApplyViewportEffect(
      controller_params.context, controller_params.screen_buffer,
      controller_params.screen_depth_stencil, controller_params.root_world,
      viewport_rect, target_color);
}

scoped_refptr<Viewport> ViewportImpl::Get_Viewport(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return viewport_;
}

void ViewportImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                                ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

scoped_refptr<Rect> ViewportImpl::Get_Rect(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return rect_;
}

void ViewportImpl::Put_Rect(const scoped_refptr<Rect>& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *rect_ = *RectImpl::From(value);
}

bool ViewportImpl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return node_.GetVisibility();
}

void ViewportImpl::Put_Visible(const bool& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeVisibility(value);
}

int32_t ViewportImpl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return static_cast<int32_t>(node_.GetSortKeys()->weight[0]);
}

void ViewportImpl::Put_Z(const int32_t& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeSortWeight(value);
}

int32_t ViewportImpl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.x;
}

void ViewportImpl::Put_Ox(const int32_t& value,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.x = value;
}

int32_t ViewportImpl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.y;
}

void ViewportImpl::Put_Oy(const int32_t& value,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.y = value;
}

scoped_refptr<Color> ViewportImpl::Get_Color(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return color_;
}

void ViewportImpl::Put_Color(const scoped_refptr<Color>& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *color_ = *ColorImpl::From(value);
}

scoped_refptr<Tone> ViewportImpl::Get_Tone(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return tone_;
}

void ViewportImpl::Put_Tone(const scoped_refptr<Tone>& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *tone_ = *ToneImpl::From(value);
}

void ViewportImpl::OnObjectDisposed() {
  node_.DisposeNode();

  Agent empty_agent;
  std::swap(agent_, empty_agent);

  viewport_.reset();
}

void ViewportImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  // Check flash state
  if (flash_emitter_.IsFlashing() && flash_emitter_.IsInvalid())
    return;

  // Current viewport region
  base::Rect viewport_rect = rect_->AsBaseRect();

  // Apply parent offset
  viewport_rect.x += params->viewport.x - params->origin.x;
  viewport_rect.y += params->viewport.y - params->origin.y;

  // Real scissor region on "screen" buffer
  viewport_rect = base::MakeIntersect(params->viewport, viewport_rect);

  // Skip render if no interaction
  if (!viewport_rect.width || !viewport_rect.height)
    return;

  // Setup new render params at current node
  DrawableNode::RenderControllerParams transient_params = *params;
  transient_params.viewport = viewport_rect;
  transient_params.origin = origin_;

  if (stage == DrawableNode::BEFORE_RENDER) {
    // Calculate viewport real offset
    base::Vec2i real_offset = viewport_rect.Position() - origin_;

    // Update self transform world uniform
    base::Rect transform_cache_rect(real_offset, params->screen_size);
    if (!(transform_cache_ == transform_cache_rect)) {
      transform_cache_ = transform_cache_rect;
      GPUUpdateViewportTransform(params->context, transform_cache_rect);
    }

    // Update effect texture size if need
    if (!(effect_layer_cache_ == viewport_rect.Size())) {
      effect_layer_cache_ = viewport_rect.Size();
      GPUResetIntermediateLayer(viewport_rect.Size());
    }

    // Notify children for preparing rendering
    controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                      &transient_params);
  } else if (stage == DrawableNode::ON_RENDERING) {
    // Setup viewport world uniform
    transient_params.world_binding = agent_.world_uniform;

    // Setup current viewport scissor
    GPUResetViewportRegion(params->context, viewport_rect);

    // Notify children for executing rendering commands
    controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                      &transient_params);

    // Restore scissor and apply viewport effect
    base::Vec4 composite_color = color_->AsNormColor();
    base::Vec4 flash_color = flash_emitter_.GetColor();
    base::Vec4 target_color = composite_color;
    if (flash_emitter_.IsFlashing())
      target_color =
          (flash_color.w > composite_color.w ? flash_color : composite_color);

    GPUViewportProcessAfterRender(
        params->context, params->root_world, params->screen_buffer,
        params->screen_depth_stencil, viewport_rect, target_color);
  }
}

void ViewportImpl::GPUCreateViewportAgent() {
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::WorldTransform),
      "viewport.world", &agent_.world_uniform, Diligent::USAGE_DEFAULT);
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::Binding_Flat::Params),
      "viewport.effect", &agent_.effect.uniform_buffer,
      Diligent::USAGE_DEFAULT);

  agent_.effect.quads = renderer::QuadBatch::Make(**context()->render_device);
  agent_.effect.binding =
      context()->render_device->GetPipelines()->viewport.CreateBinding();
}

void ViewportImpl::GPUUpdateViewportTransform(
    renderer::RenderContext* render_context,
    const base::Rect& region) {
  renderer::WorldTransform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, region.Size());
  renderer::MakeTransformMatrix(world_matrix.transform, region.Size(),
                                region.Position());

  (*render_context)
      ->UpdateBuffer(agent_.world_uniform, 0, sizeof(world_matrix),
                     &world_matrix,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ViewportImpl::GPUResetIntermediateLayer(const base::Vec2i& effect_size) {
  auto effect_layer = agent_.effect.intermediate_layer;
  if (!effect_layer ||
      static_cast<int32_t>(effect_layer->GetDesc().Width) < effect_size.x ||
      static_cast<int32_t>(effect_layer->GetDesc().Height) < effect_size.y) {
    agent_.effect.layer_size.x = std::max<int32_t>(
        effect_size.x, effect_layer ? effect_layer->GetDesc().Width : 0);
    agent_.effect.layer_size.y = std::max<int32_t>(
        effect_size.y, effect_layer ? effect_layer->GetDesc().Height : 0);

    agent_.effect.intermediate_layer.Release();
    renderer::CreateTexture2D(
        **context()->render_device, &agent_.effect.intermediate_layer,
        "viewport.intermediate.layer", agent_.effect.layer_size);
  }
}

void ViewportImpl::GPUResetViewportRegion(
    renderer::RenderContext* render_context,
    const base::Rect& region) {
  render_context->ScissorState()->Push(region);
}

void ViewportImpl::GPUApplyViewportEffect(
    renderer::RenderContext* render_context,
    Diligent::ITexture* screen_buffer,
    Diligent::ITexture* screen_depth_stencil,
    Diligent::IBuffer* root_world,
    const base::Rect& effect_region,
    const base::Vec4& color) {
  // Copy "screen" buffer desired region data to stage intermediate texture
  Diligent::Box box;
  box.MinX = effect_region.x;
  box.MinY = effect_region.y;
  box.MaxX = effect_region.x + effect_region.width;
  box.MaxY = effect_region.y + effect_region.height;

  (*render_context)
      ->SetRenderTargets(0, nullptr, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::CopyTextureAttribs copy_texture_attribs;
  copy_texture_attribs.pSrcTexture = screen_buffer;
  copy_texture_attribs.pSrcBox = &box;
  copy_texture_attribs.SrcTextureTransitionMode =
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copy_texture_attribs.pDstTexture = agent_.effect.intermediate_layer;
  copy_texture_attribs.DstTextureTransitionMode =
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  (*render_context)->CopyTexture(copy_texture_attribs);

  // Make effect transient vertices
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad, effect_region);
  renderer::Quad::SetTexCoordRect(&transient_quad,
                                  base::Rect(effect_region.Size()),
                                  agent_.effect.layer_size);
  agent_.effect.quads.QueueWrite(**render_context, &transient_quad);

  // Update uniform data
  renderer::Binding_Flat::Params transient_uniform;
  transient_uniform.Color = color;
  transient_uniform.Tone = tone_->AsNormColor();
  (*render_context)
      ->UpdateBuffer(agent_.effect.uniform_buffer, 0, sizeof(transient_uniform),
                     &transient_uniform,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Derive effect pipeline
  auto& pipeline_set = context()->render_device->GetPipelines()->viewport;
  auto* pipeline =
      pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND, false);

  // Apply render target
  Diligent::ITextureView* render_target_view =
      screen_buffer->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  (*render_context)
      ->SetRenderTargets(1, &render_target_view, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup uniform params
  agent_.effect.binding.u_transform->Set(root_world);
  agent_.effect.binding.u_texture->Set(
      agent_.effect.intermediate_layer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.effect.binding.u_params->Set(agent_.effect.uniform_buffer);

  // Apply pipeline state
  (*render_context)->SetPipelineState(pipeline);
  (*render_context)
      ->CommitShaderResources(
          *agent_.effect.binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.effect.quads;
  (*render_context)
      ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  (*render_context)->DrawIndexed(draw_indexed_attribs);

  // Restore depth test buffer
  Diligent::ITextureView* depth_stencil_view =
      screen_depth_stencil->GetDefaultView(
          Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
  (*render_context)
      ->SetRenderTargets(1, &render_target_view, depth_stencil_view,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ViewportImpl::GPUViewportProcessAfterRender(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* root_world,
    Diligent::ITexture* screen_buffer,
    Diligent::ITexture* screen_depth_stencil,
    const base::Rect& effect_region,
    const base::Vec4& color) {
  const auto tone = tone_->AsNormColor();
  const bool is_effect_valid =
      color.w != 0 || tone.x != 0 || tone.y != 0 || tone.z != 0 || tone.w != 0;
  if (is_effect_valid)
    GPUApplyViewportEffect(render_context, screen_buffer, screen_depth_stencil,
                           root_world, effect_region, color);

  // Restore viewport region
  render_context->ScissorState()->Pop();
}

void ViewportImpl::GPUFrameBeginRenderPassInternal(
    renderer::RenderContext* render_context,
    BitmapAgent* render_target,
    const base::Vec2i& viewport_offset,
    const base::Rect& scissor_region) {
  // Apply render target
  (*render_context)
      ->SetRenderTargets(1, &render_target->target, render_target->depth_view,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup scissor region
  render_context->ScissorState()->Apply(scissor_region.Size());
}

}  // namespace content
