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
                                "viewport.world", &agent->world_uniform);
  Diligent::CreateUniformBuffer(
      **device, sizeof(renderer::Binding_Flat::Params), "viewport.effect",
      &agent->effect.uniform_buffer);

  Diligent::BufferViewDesc buffer_view_desc;
  buffer_view_desc.ViewType = Diligent::BUFFER_VIEW_SHADER_RESOURCE;
  agent->world_uniform->CreateView(buffer_view_desc, &agent->world_binding);
  agent->effect.uniform_buffer->CreateView(buffer_view_desc,
                                           &agent->effect.uniform_binding);

  agent->effect.quads = renderer::QuadBatch::Make(**device);
  agent->effect.binding =
      device->GetPipelines()->viewport.CreateBinding<renderer::Binding_Flat>();
}

void GPUDestroyViewportAgent(ViewportAgent* agent) {
  delete agent;
}

void GPUUpdateViewportAgentData(renderer::RenderDevice* device,
                                ViewportAgent* agent,
                                const base::Vec2i& screen_size,
                                const base::Vec2i& viewport_offset,
                                const base::Vec2i& effect_size) {
  auto* context = device->GetContext();

  // Reload viewport offset based transform matrix
  renderer::WorldTransform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, screen_size,
                                 viewport_offset);
  renderer::MakeIdentityMatrix(world_matrix.transform);

  context->UpdateBuffer(agent->world_uniform, 0, sizeof(world_matrix),
                        &world_matrix,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Reset viewport effect intermediate texture layer if needed
  auto effect_layer = agent->effect.intermediate_layer;
  if (!effect_layer || effect_layer->GetDesc().Width < effect_size.x ||
      effect_layer->GetDesc().Height < effect_size.y) {
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

void GPUResetViewportRegion(renderer::RenderDevice* device,
                            const base::Rect& region) {
  Diligent::Rect render_scissor;
  render_scissor.left = region.x;
  render_scissor.top = region.y;
  render_scissor.right = region.x + region.width;
  render_scissor.bottom = region.y + region.height;
  device->GetContext()->SetScissorRects(
      1, &render_scissor, 1, render_scissor.bottom + render_scissor.top);
}

void GPUApplyViewportEffect(renderer::RenderDevice* device,
                            Diligent::ITexture* screen_buffer,
                            Diligent::IBufferView* root_world,
                            ViewportAgent* agent,
                            const base::Rect& effect_region,
                            const base::Vec4& color,
                            const base::Vec4& tone) {
  auto* context = device->GetContext();

  Diligent::Box box;
  box.MinX = effect_region.x;
  box.MinY = effect_region.y;
  box.MaxX = effect_region.x + effect_region.width;
  box.MaxY = effect_region.y + effect_region.height;

  Diligent::CopyTextureAttribs copy_texture_attribs;
  copy_texture_attribs.pSrcTexture = screen_buffer;
  copy_texture_attribs.pSrcBox = &box;
  copy_texture_attribs.pDstTexture = agent->effect.intermediate_layer;
  context->CopyTexture(copy_texture_attribs);

  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad, effect_region);
  renderer::Quad::SetTexCoordRect(&transient_quad,
                                  base::Rect(effect_region.Size()),
                                  agent->effect.layer_size);
  agent->effect.quads->QueueWrite(context, &transient_quad);

  renderer::Binding_Flat::Params transient_uniform;
  transient_uniform.color = color;
  transient_uniform.tone = tone;
  context->UpdateBuffer(agent->effect.uniform_buffer, 0,
                        sizeof(transient_uniform), &transient_uniform,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& pipeline_set = device->GetPipelines()->viewport;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  // Apply render target
  Diligent::ITextureView* render_target_view =
      screen_buffer->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup uniform params
  agent->effect.binding->u_transform->Set(root_world);
  agent->effect.binding->u_texture->Set(
      agent->effect.intermediate_layer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent->effect.binding->u_params->Set(agent->effect.uniform_binding);

  // Apply pipeline state
  context->SetPipelineState(pipeline);
  context->CommitShaderResources(
      **agent->effect.binding,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = **agent->effect.quads;
  context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->SetIndexBuffer(**device->GetQuadIndex(), 0,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = device->GetQuadIndex()->format();
  context->DrawIndexed(draw_indexed_attribs);
}

void GPUViewportProcessAfterRender(renderer::RenderDevice* device,
                                   Diligent::IBufferView* root_world,
                                   const base::Rect& last_viewport,
                                   Diligent::ITexture* screen_buffer,
                                   ViewportAgent* agent,
                                   const base::Rect& effect_region,
                                   const base::Vec4& color,
                                   const base::Vec4& tone) {
  const bool is_effect_valid =
      color.w != 0 || tone.x != 0 || tone.y != 0 || tone.z != 0 || tone.w != 0;

  if (is_effect_valid)
    GPUApplyViewportEffect(device, screen_buffer, root_world, agent,
                           effect_region, color, tone);

  // Restore viewport region
  Diligent::Rect render_scissor;
  render_scissor.left = last_viewport.x;
  render_scissor.top = last_viewport.y;
  render_scissor.right = last_viewport.x + last_viewport.width;
  render_scissor.bottom = last_viewport.y + last_viewport.height;
  device->GetContext()->SetScissorRects(
      1, &render_scissor, 1, render_scissor.bottom + render_scissor.top);
}

void GPUFrameBeginRenderPassInternal(renderer::RenderDevice* device,
                                     ViewportAgent* agent,
                                     Diligent::ITexture* render_target,
                                     const base::Vec2i& offset) {
  auto* context = device->GetContext();
  base::Vec2i target_size(render_target->GetDesc().Width,
                          render_target->GetDesc().Height);

  // Reload viewport offset based transform matrix
  renderer::WorldTransform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, target_size, offset);
  renderer::MakeIdentityMatrix(world_matrix.transform);

  context->UpdateBuffer(agent->world_uniform, 0, sizeof(world_matrix),
                        &world_matrix,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply render target
  Diligent::ITextureView* render_target_view =
      render_target->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

}  // namespace

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      ExceptionState& exception_state) {
  return new ViewportImpl(execution_context->graphics, nullptr,
                          execution_context->graphics->GetResolution());
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  return new ViewportImpl(execution_context->graphics, nullptr,
                          RectImpl::From(rect)->AsBaseRect());
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      ExceptionState& exception_state) {
  return new ViewportImpl(execution_context->graphics, nullptr,
                          base::Rect(x, y, width, height));
}

scoped_refptr<Viewport> Viewport::New(ExecutionContext* execution_context,
                                      scoped_refptr<Viewport> parent,
                                      scoped_refptr<Rect> rect,
                                      ExceptionState& exception_state) {
  scoped_refptr<RectImpl> region = RectImpl::From(rect);
  return new ViewportImpl(execution_context->graphics,
                          ViewportImpl::From(parent), region->AsBaseRect());
}

ViewportImpl::ViewportImpl(RenderScreenImpl* screen,
                           scoped_refptr<ViewportImpl> parent,
                           const base::Rect& region)
    : GraphicsChild(screen),
      Disposable(screen),
      node_(parent ? parent->GetDrawableController()
                   : screen->GetDrawableController(),
            SortKey()),
      rect_(new RectImpl(region)),
      color_(new ColorImpl(base::Vec4())),
      tone_(new ToneImpl(base::Vec4())) {
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
  if (!bitmap_agent) {
    exception_state.ThrowContentError(
        ExceptionCode::CONTENT_ERROR,
        "Viewport.Render: Invalid bitmap as render target.");
    return;
  }

  // Submit pending canvas commands
  screen()->GetCanvasScheduler()->SubmitPendingPaintCommands();

  // Check flash status
  if (flash_emitter_.IsFlashing() && flash_emitter_.IsInvalid())
    return;

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.device = screen()->GetDevice();
  controller_params.screen_buffer = bitmap_agent->data;
  controller_params.screen_size = bitmap_agent->size;
  controller_params.viewport = bitmap_agent->size;
  controller_params.origin = base::Vec2i();

  // 1) Execute pre-composite handler
  controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 1.5) Update sprite batch
  screen()->PostTask(base::BindOnce(
      &SpriteBatch::SubmitBatchDataAndResetCache,
      base::Unretained(screen()->GetSpriteBatch()), controller_params.device));

  // 2) Setup renderpass
  base::Rect viewport_rect = rect_->AsBaseRect();
  base::Vec2i offset = viewport_rect.Position() - origin_;
  agent_->region_cache = base::Rect(offset, bitmap_agent->size);
  screen()->PostTask(base::BindOnce(
      &GPUFrameBeginRenderPassInternal, controller_params.device, agent_,
      base::Unretained(bitmap_agent->data.RawPtr()), offset));

  // 3) Notify render a frame
  controller_params.root_world = bitmap_agent->world_binding;
  controller_params.world_binding = agent_->world_binding;
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
                     base::Unretained(controller_params.screen_buffer),
                     base::Unretained(controller_params.root_world), agent_,
                     viewport_rect, target_color, tone_->AsNormColor()));

  screen()->WaitWorkerSynchronize();
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
  // Check flash status
  if (flash_emitter_.IsFlashing() && flash_emitter_.IsInvalid())
    return;

  // Stack storage last render state
  DrawableNode::RenderControllerParams transient_params = *params;

  // Stack of viewport region rect
  base::Rect viewport_rect = rect_->AsBaseRect();
  viewport_rect.x += params->viewport.x;
  viewport_rect.y += params->viewport.y;
  transient_params.viewport = viewport_rect;
  transient_params.origin = origin_;

  // Check render visible
  viewport_rect = base::MakeIntersect(params->viewport, viewport_rect);
  if (!viewport_rect.width || !viewport_rect.height)
    return;

  if (stage == DrawableNode::BEFORE_RENDER) {
    // Calculate viewport offset
    base::Vec2i offset = viewport_rect.Position() - origin_;

    // Update uniform buffer if viewport region changed.
    base::Rect cache_rect(offset, viewport_rect.Size());
    if (!(agent_->region_cache == cache_rect)) {
      agent_->region_cache = cache_rect;
      screen()->PostTask(
          base::BindOnce(&GPUUpdateViewportAgentData, params->device, agent_,
                         params->screen_size, offset, viewport_rect.Size()));
    }
  }

  if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    // Setup viewport's world settings
    transient_params.world_binding = agent_->world_binding;

    // Set new viewport
    screen()->PostTask(
        base::BindOnce(&GPUResetViewportRegion, params->device, viewport_rect));
  }

  // Notify children drawable node
  controller_.BroadCastNotification(stage, &transient_params);

  // Restore last viewport settings and apply effect
  if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    base::Vec4 composite_color = color_->AsNormColor();
    base::Vec4 flash_color = flash_emitter_.GetColor();
    base::Vec4 target_color = composite_color;
    if (flash_emitter_.IsFlashing())
      target_color =
          (flash_color.w > composite_color.w ? flash_color : composite_color);

    screen()->PostTask(
        base::BindOnce(&GPUViewportProcessAfterRender, params->device,
                       base::Unretained(params->root_world), params->viewport,
                       base::Unretained(params->screen_buffer), agent_,
                       viewport_rect, target_color, tone_->AsNormColor()));
  }
}

}  // namespace content
