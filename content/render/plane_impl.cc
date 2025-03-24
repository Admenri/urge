// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/plane_impl.h"

#include <algorithm>

namespace content {

namespace {

void GPUCreatePlaneInternal(renderer::RenderDevice* device, PlaneAgent* agent) {
  agent->batch = renderer::QuadBatch::Make(**device, 0);
  agent->uniform_buffer =
      renderer::CreateUniformBuffer<renderer::ViewportFragmentUniform>(
          **device, "plane.uniform", wgpu::BufferUsage::CopyDst);
  agent->uniform_binding = renderer::ViewportFragmentUniform::CreateGroup(
      **device, agent->uniform_buffer);
}

void GPUDestroyPlaneInternal(PlaneAgent* agent) {
  delete agent;
}

void GPUUpdatePlaneQuadArrayInternal(renderer::RenderDevice* device,
                                     wgpu::CommandEncoder* command_encoder,
                                     PlaneAgent* agent,
                                     TextureAgent* texture,
                                     const base::Vec2i& viewport_size,
                                     const base::Vec2& scale,
                                     const base::Vec2i& origin,
                                     const base::Vec4& color,
                                     const base::Vec4& tone,
                                     int32_t opacity) {
  // Pre-calculate tile dimensions with scaling
  const float item_x =
      std::max(1.0f, static_cast<float>(texture->size.x) * scale.x);
  const float item_y =
      std::max(1.0f, static_cast<float>(texture->size.y) * scale.y);

  // Wrap float into desire value
  auto value_wrap = [](float value, float range) {
    float result = std::fmod(value, range);
    return result < 0 ? result + range : result;
  };

  // Calculate wrapped origin offsets
  const float wrap_ox = value_wrap(origin.x, item_x);
  const float wrap_oy = value_wrap(origin.y, item_y);

  // Optimized tile calculation using simplified formula
  const float total_x = static_cast<float>(viewport_size.x) + wrap_ox;
  const float total_y = static_cast<float>(viewport_size.y) + wrap_oy;
  const int tile_x = static_cast<int>(std::ceil(total_x / item_x));
  const int tile_y = static_cast<int>(std::ceil(total_y / item_y));

  // Prepare vertex buffer
  const int quad_size = tile_x * tile_y;
  agent->cache.resize(quad_size);
  const base::Vec4 opacity_norm(static_cast<float>(opacity) / 255.0f);

  // Pointer-based vertex writing with accumulative positioning
  renderer::Quad* quad_ptr = agent->cache.data();
  float current_y = -wrap_oy;
  for (int y = 0; y < tile_y; ++y) {
    float current_x = -wrap_ox;
    for (int x = 0; x < tile_x; ++x) {
      // Set vertex properties directly through pointer
      const base::RectF pos(current_x, current_y, item_x, item_y);
      renderer::Quad::SetPositionRect(quad_ptr, pos);
      renderer::Quad::SetTexCoordRect(quad_ptr, base::Vec2(item_x, item_y));
      renderer::Quad::SetColor(quad_ptr, opacity_norm);

      // Move to next quad using pointer arithmetic
      ++quad_ptr;
      current_x += item_x;  // X-axis accumulation
    }
    current_y += item_y;  // Y-axis accumulation
  }

  device->GetQuadIndex()->Allocate(quad_size);
  agent->quad_size = quad_size;
  agent->batch->QueueWrite(*command_encoder, agent->cache.data(),
                           agent->cache.size());

  renderer::ViewportFragmentUniform transient_uniform;
  transient_uniform.color = color;
  transient_uniform.tone = tone;
  command_encoder->WriteBuffer(agent->uniform_buffer, 0,
                               reinterpret_cast<uint8_t*>(&transient_uniform),
                               sizeof(transient_uniform));
}

void GPUOnViewportRenderingInternal(renderer::RenderDevice* device,
                                    wgpu::RenderPassEncoder* encoder,
                                    wgpu::BindGroup* world_binding,
                                    PlaneAgent* agent,
                                    TextureAgent* texture,
                                    int32_t blend_type) {
  auto& pipeline_set = device->GetPipelines()->viewport;
  auto* pipeline =
      pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type));

  encoder->SetPipeline(*pipeline);
  encoder->SetVertexBuffer(0, **agent->batch);
  encoder->SetIndexBuffer(**device->GetQuadIndex(),
                          device->GetQuadIndex()->format());
  encoder->SetBindGroup(0, *world_binding);
  encoder->SetBindGroup(1, texture->binding);
  encoder->SetBindGroup(2, agent->uniform_binding);
  encoder->DrawIndexed(agent->quad_size * 6);
}

}  // namespace

scoped_refptr<Plane> Plane::New(ExecutionContext* execution_context,
                                scoped_refptr<Viewport> viewport,
                                ExceptionState& exception_state) {
  return new PlaneImpl(execution_context->graphics,
                       ViewportImpl::From(viewport));
}

PlaneImpl::PlaneImpl(RenderScreenImpl* screen,
                     scoped_refptr<ViewportImpl> parent)
    : GraphicsChild(screen),
      Disposable(screen),
      node_(parent ? parent->GetDrawableController()
                   : screen->GetDrawableController(),
            SortKey()),
      viewport_(parent),
      color_(new ColorImpl(base::Vec4())),
      tone_(new ToneImpl(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &PlaneImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  agent_ = new PlaneAgent;
  screen->PostTask(
      base::BindOnce(&GPUCreatePlaneInternal, screen->GetDevice(), agent_));
}

PlaneImpl::~PlaneImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void PlaneImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool PlaneImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

scoped_refptr<Bitmap> PlaneImpl::Get_Bitmap(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return bitmap_;
}

void PlaneImpl::Put_Bitmap(const scoped_refptr<Bitmap>& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bitmap_ = CanvasImpl::FromBitmap(value);
  quad_array_dirty_ = true;
}

scoped_refptr<Viewport> PlaneImpl::Get_Viewport(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return viewport_;
}

void PlaneImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_->GetDrawableController());
  quad_array_dirty_ = true;
}

bool PlaneImpl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return node_.GetVisibility();
}

void PlaneImpl::Put_Visible(const bool& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeVisibility(value);
}

int32_t PlaneImpl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return node_.GetSortKeys()->weight[0];
}

void PlaneImpl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeSortWeight(value);
}

int32_t PlaneImpl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.x;
}

void PlaneImpl::Put_Ox(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.x = value;
  quad_array_dirty_ = true;
}

int32_t PlaneImpl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.y;
}

void PlaneImpl::Put_Oy(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.y = value;
  quad_array_dirty_ = true;
}

float PlaneImpl::Get_ZoomX(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return scale_.x;
}

void PlaneImpl::Put_ZoomX(const float& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  scale_.x = value;
  quad_array_dirty_ = true;
}

float PlaneImpl::Get_ZoomY(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return scale_.y;
}

void PlaneImpl::Put_ZoomY(const float& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  scale_.y = value;
  quad_array_dirty_ = true;
}

int32_t PlaneImpl::Get_Opacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return opacity_;
}

void PlaneImpl::Put_Opacity(const int32_t& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t PlaneImpl::Get_BlendType(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return blend_type_;
}

void PlaneImpl::Put_BlendType(const int32_t& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  blend_type_ = value;
}

scoped_refptr<Color> PlaneImpl::Get_Color(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return color_;
}

void PlaneImpl::Put_Color(const scoped_refptr<Color>& value,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  color_ = ColorImpl::From(value);
}

scoped_refptr<Tone> PlaneImpl::Get_Tone(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return tone_;
}

void PlaneImpl::Put_Tone(const scoped_refptr<Tone>& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  tone_ = ToneImpl::From(value);
}

void PlaneImpl::OnObjectDisposed() {
  node_.DisposeNode();

  screen()->PostTask(base::BindOnce(&GPUDestroyPlaneInternal, agent_));
  agent_ = nullptr;
}

void PlaneImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (!bitmap_)
    return;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    if (quad_array_dirty_) {
      quad_array_dirty_ = false;
      screen()->PostTask(base::BindOnce(
          &GPUUpdatePlaneQuadArrayInternal, params->device,
          params->command_encoder, agent_, bitmap_->GetAgent(),
          params->viewport.Size(), scale_, origin_, color_->AsNormColor(),
          tone_->AsNormColor(), opacity_));
    }
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    screen()->PostTask(
        base::BindOnce(&GPUOnViewportRenderingInternal, params->device,
                       params->renderpass_encoder, params->world_binding,
                       agent_, bitmap_->GetAgent(), blend_type_));
  }
}

}  // namespace content
