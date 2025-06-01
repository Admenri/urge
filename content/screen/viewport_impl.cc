// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/viewport_impl.h"

#include "content/canvas/canvas_impl.h"
#include "content/canvas/canvas_scheduler.h"
#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

void GPUCreateViewportAgent(renderer::RenderDevice* device,
                            ViewportAgent* agent) {
  Diligent::CreateUniformBuffer(**device, sizeof(renderer::WorldTransform),
                                "viewport.world", &agent->world_uniform,
                                Diligent::USAGE_DEFAULT);
  Diligent::CreateUniformBuffer(
      **device, sizeof(renderer::Binding_Flat::Params), "viewport.effect",
      &agent->effect.uniform_buffer, Diligent::USAGE_DEFAULT);

  agent->effect.quads = renderer::QuadBatch::Make(**device);
  agent->effect.binding = device->GetPipelines()->viewport.CreateBinding();
}

void GPUDestroyViewportAgent(ViewportAgent* agent) {
  delete agent;
}

void GPUUpdateViewportTransform(renderer::RenderDevice* device,
                                renderer::RenderContext* context,
                                ViewportAgent* agent,
                                const base::Rect& region) {
  renderer::WorldTransform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, region.Size());
  renderer::MakeTransformMatrix(world_matrix.transform, region.Size(),
                                region.Position());

  (*context)->UpdateBuffer(agent->world_uniform, 0, sizeof(world_matrix),
                           &world_matrix,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void GPUResetIntermediateLayer(renderer::RenderDevice* device,
                               ViewportAgent* agent,
                               const base::Vec2i& effect_size) {
  auto effect_layer = agent->effect.intermediate_layer;
  if (!effect_layer ||
      static_cast<int32_t>(effect_layer->GetDesc().Width) < effect_size.x ||
      static_cast<int32_t>(effect_layer->GetDesc().Height) < effect_size.y) {
    agent->effect.layer_size.x = std::max<int32_t>(
        effect_size.x, effect_layer ? effect_layer->GetDesc().Width : 0);
    agent->effect.layer_size.y = std::max<int32_t>(
        effect_size.y, effect_layer ? effect_layer->GetDesc().Height : 0);

    agent->effect.intermediate_layer.Release();
    renderer::CreateTexture2D(**device, &agent->effect.intermediate_layer,
                              "viewport.intermediate.layer",
                              agent->effect.layer_size);
  }
}

void GPUResetViewportRegion(renderer::RenderContext* context,
                            const base::Rect& region) {
  context->ScissorState()->Push(region);
}

void GPUApplyViewportEffect(renderer::RenderDevice* device,
                            renderer::RenderContext* context,
                            Diligent::ITexture** screen_buffer,
                            Diligent::IBuffer** root_world,
                            ViewportAgent* agent,
                            const base::Rect& effect_region,
                            const base::Vec4& color,
                            const base::Vec4& tone) {
  // Copy "screen" buffer desired region data to stage intermediate texture
  Diligent::Box box;
  box.MinX = effect_region.x;
  box.MinY = effect_region.y;
  box.MaxX = effect_region.x + effect_region.width;
  box.MaxY = effect_region.y + effect_region.height;

  (*context)->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::CopyTextureAttribs copy_texture_attribs;
  copy_texture_attribs.pSrcTexture = *screen_buffer;
  copy_texture_attribs.pSrcBox = &box;
  copy_texture_attribs.SrcTextureTransitionMode =
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copy_texture_attribs.pDstTexture = agent->effect.intermediate_layer;
  copy_texture_attribs.DstTextureTransitionMode =
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  (*context)->CopyTexture(copy_texture_attribs);

  // Make effect transient vertices
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad, effect_region);
  renderer::Quad::SetTexCoordRect(&transient_quad,
                                  base::Rect(effect_region.Size()),
                                  agent->effect.layer_size);
  agent->effect.quads->QueueWrite(**context, &transient_quad);

  // Update uniform data
  renderer::Binding_Flat::Params transient_uniform;
  transient_uniform.Color = color;
  transient_uniform.Tone = tone;
  (*context)->UpdateBuffer(agent->effect.uniform_buffer, 0,
                           sizeof(transient_uniform), &transient_uniform,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Derive effect pipeline
  auto& pipeline_set = device->GetPipelines()->viewport;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  // Apply render target
  Diligent::ITextureView* render_target_view =
      (*screen_buffer)->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  (*context)->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup uniform params
  agent->effect.binding->u_transform->Set(*root_world);
  agent->effect.binding->u_texture->Set(
      agent->effect.intermediate_layer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent->effect.binding->u_params->Set(agent->effect.uniform_buffer);

  // Apply pipeline state
  (*context)->SetPipelineState(pipeline);
  (*context)->CommitShaderResources(
      **agent->effect.binding,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = **agent->effect.quads;
  (*context)->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*context)->SetIndexBuffer(
      **device->GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  (*context)->DrawIndexed(draw_indexed_attribs);
}

void GPUViewportProcessAfterRender(renderer::RenderDevice* device,
                                   renderer::RenderContext* context,
                                   Diligent::IBuffer** root_world,
                                   Diligent::ITexture** screen_buffer,
                                   ViewportAgent* agent,
                                   const base::Rect& effect_region,
                                   const base::Vec4& color,
                                   const base::Vec4& tone) {
  const bool is_effect_valid =
      color.w != 0 || tone.x != 0 || tone.y != 0 || tone.z != 0 || tone.w != 0;
  if (is_effect_valid)
    GPUApplyViewportEffect(device, context, screen_buffer, root_world, agent,
                           effect_region, color, tone);

  // Restore viewport region
  context->ScissorState()->Pop();
}

void GPUFrameBeginRenderPassInternal(renderer::RenderDevice* device,
                                     renderer::RenderContext* context,
                                     ViewportAgent* agent,
                                     Diligent::ITexture** render_target,
                                     const base::Vec2i& viewport_offset,
                                     const base::Rect& scissor_region) {
  // Apply render target
  Diligent::ITextureView* render_target_view =
      (*render_target)->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  (*context)->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup scissor region
  context->ScissorState()->Apply(scissor_region.Size());
}

}  // namespace

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(
      execution_context->graphics, nullptr,
      execution_context->graphics->GetResolution());
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(
      execution_context->graphics, nullptr, RectImpl::From(rect)->AsBaseRect());
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<ViewportImpl>(
      execution_context->graphics, nullptr, base::Rect(x, y, width, height));
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Viewport> parent,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  scoped_refptr<RectImpl> region = RectImpl::From(rect);
  return base::MakeRefCounted<ViewportImpl>(execution_context->graphics,
                                            ViewportImpl::From(parent),
                                            region->AsBaseRect());
}

ViewportImpl::ViewportImpl(RenderScreenImpl* screen,
                           scoped_refptr<ViewportImpl> parent,
                           const base::Rect& region)
    : GraphicsChild(screen),
      Disposable(screen),
      node_(parent ? parent->GetDrawableController()
                   : screen->GetDrawableController(),
            SortKey()),
      rect_(base::MakeRefCounted<RectImpl>(region)),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &ViewportImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  agent_ = new ViewportAgent;
  screen->PostTask(
      base::BindOnce(&GPUCreateViewportAgent, screen->GetDevice(), agent_));
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
  TextureAgent* bitmap_agent =
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
  screen()->GetCanvasScheduler()->SubmitPendingPaintCommands();

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.device = screen()->GetDevice();
  controller_params.context = screen()->GetContext();
  controller_params.screen_buffer = bitmap_agent->data.RawDblPtr();
  controller_params.screen_size = bitmap_agent->size;
  controller_params.viewport = viewport_rect;
  controller_params.origin = origin_;

  // 0) Update uniform buffer if viewport region changed
  base::Rect transform_cache_rect(offset, controller_params.screen_size);
  if (!(transform_cache_ == transform_cache_rect)) {
    transform_cache_ = transform_cache_rect;
    screen()->PostTask(base::BindOnce(
        &GPUUpdateViewportTransform, controller_params.device,
        controller_params.context, agent_, transform_cache_rect));
  }

  // 0.5) Reset intermediate layer if need
  if (!(effect_layer_cache_ == viewport_rect.Size())) {
    effect_layer_cache_ = viewport_rect.Size();
    screen()->PostTask(base::BindOnce(&GPUResetIntermediateLayer,
                                      controller_params.device, agent_,
                                      viewport_rect.Size()));
  }

  // 1) Execute pre-composite handler
  controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 1.5) Update sprite batch if need
  if (screen()->GetSpriteBatch())
    screen()->PostTask(
        base::BindOnce(&SpriteBatch::SubmitBatchDataAndResetCache,
                       base::Unretained(screen()->GetSpriteBatch()),
                       controller_params.device, controller_params.context));

  // 2) Setup renderpass
  screen()->PostTask(
      base::BindOnce(&GPUFrameBeginRenderPassInternal, controller_params.device,
                     controller_params.context, agent_,
                     bitmap_agent->data.RawDblPtr(), offset, viewport_rect));

  // 3) Notify render a frame
  controller_params.root_world = bitmap_agent->world_buffer.RawDblPtr();
  controller_params.world_binding = agent_->world_uniform.RawDblPtr();
  controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  base::Vec4 composite_color = color_->AsNormColor();
  base::Vec4 flash_color = flash_emitter_.GetColor();
  base::Vec4 target_color = composite_color;
  if (flash_emitter_.IsFlashing())
    target_color =
        (flash_color.w > composite_color.w ? flash_color : composite_color);

  screen()->PostTask(
      base::BindOnce(&GPUApplyViewportEffect, controller_params.device,
                     controller_params.context, controller_params.screen_buffer,
                     controller_params.root_world, agent_, viewport_rect,
                     target_color, tone_->AsNormColor()));
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
                                   : screen()->GetDrawableController());
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

  screen()->PostTask(base::BindOnce(&GPUDestroyViewportAgent, agent_));
  agent_ = nullptr;

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
      screen()->PostTask(base::BindOnce(&GPUUpdateViewportTransform,
                                        params->device, params->context, agent_,
                                        transform_cache_rect));
    }

    // Update effect texture size if need
    if (!(effect_layer_cache_ == viewport_rect.Size())) {
      effect_layer_cache_ = viewport_rect.Size();
      screen()->PostTask(base::BindOnce(&GPUResetIntermediateLayer,
                                        params->device, agent_,
                                        viewport_rect.Size()));
    }

    // Notify children for preparing rendering
    controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                      &transient_params);
  } else if (stage == DrawableNode::ON_RENDERING) {
    // Setup viewport world uniform
    transient_params.world_binding = agent_->world_uniform.RawDblPtr();

    // Setup current viewport scissor
    screen()->PostTask(base::BindOnce(&GPUResetViewportRegion, params->context,
                                      viewport_rect));

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

    screen()->PostTask(base::BindOnce(
        &GPUViewportProcessAfterRender, params->device, params->context,
        params->root_world, params->screen_buffer, agent_, viewport_rect,
        target_color, tone_->AsNormColor()));
  }
}

}  // namespace content
