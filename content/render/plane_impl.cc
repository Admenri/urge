// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/plane_impl.h"

#include <algorithm>

#include "content/context/execution_context.h"

namespace content {

// static
scoped_refptr<Plane> Plane::New(ExecutionContext* execution_context,
                                scoped_refptr<Viewport> viewport,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<PlaneImpl>(execution_context,
                                         ViewportImpl::From(viewport));
}

///////////////////////////////////////////////////////////////////////////////
// PlaneImpl Implement

PlaneImpl::PlaneImpl(ExecutionContext* execution_context,
                     scoped_refptr<ViewportImpl> parent)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()),
      viewport_(parent),
      src_rect_(base::MakeRefCounted<RectImpl>(base::Rect())),
      scale_(1.0f),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &PlaneImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  GPUCreatePlaneInternal();
}

DISPOSABLE_DEFINITION(PlaneImpl);

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Bitmap,
    scoped_refptr<Bitmap>,
    PlaneImpl,
    { return bitmap_; },
    {
      DISPOSE_CHECK;

      bitmap_ = CanvasImpl::FromBitmap(value);
      if (Disposable::IsValid(bitmap_.get()))
        src_rect_->SetBase((**bitmap_)->size);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    SrcRect,
    scoped_refptr<Rect>,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(nullptr);
      return src_rect_;
    },
    {
      DISPOSE_CHECK;
      CHECK_ATTRIBUTE_VALUE;
      *src_rect_ = *RectImpl::From(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Viewport,
    scoped_refptr<Viewport>,
    PlaneImpl,
    { return viewport_; },
    {
      DISPOSE_CHECK;

      if (viewport_ == value)
        return;

      viewport_ = ViewportImpl::From(value);
      node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                       : context()->screen_drawable_node);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Visible,
    bool,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(false);

      return node_.GetVisibility();
    },
    {
      DISPOSE_CHECK;

      node_.SetNodeVisibility(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Z,
    int32_t,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0);

      return node_.GetSortKeys()->weight[0];
    },
    {
      DISPOSE_CHECK;

      node_.SetNodeSortWeight(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Ox,
    int32_t,
    PlaneImpl,
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
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0);

      return origin_.y;
    },
    {
      DISPOSE_CHECK;

      origin_.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    ZoomX,
    float,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);

      return scale_.x;
    },
    {
      DISPOSE_CHECK;

      scale_.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    ZoomY,
    float,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);

      return scale_.y;
    },
    {
      DISPOSE_CHECK;

      scale_.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Opacity,
    int32_t,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0);

      return opacity_;
    },
    {
      DISPOSE_CHECK;

      opacity_ = std::clamp(value, 0, 255);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    BlendType,
    int32_t,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(0);

      return blend_type_;
    },
    {
      DISPOSE_CHECK;

      blend_type_ = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Color,
    scoped_refptr<Color>,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(nullptr);

      return color_;
    },
    {
      DISPOSE_CHECK;

      CHECK_ATTRIBUTE_VALUE;

      *color_ = *ColorImpl::From(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Tone,
    scoped_refptr<Tone>,
    PlaneImpl,
    {
      DISPOSE_CHECK_RETURN(nullptr);

      return tone_;
    },
    {
      DISPOSE_CHECK;

      CHECK_ATTRIBUTE_VALUE;

      *tone_ = *ToneImpl::From(value);
    });

void PlaneImpl::OnObjectDisposed() {
  node_.DisposeNode();

  GPUData empty_data;
  std::swap(gpu_, empty_data);
}

void PlaneImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (!Disposable::IsValid(bitmap_.get()))
    return;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    GPUUpdatePlaneQuadArrayInternal(params->context, src_rect_->AsBaseRect(),
                                    node_.GetParentViewport()->bound.Size(),
                                    scale_, origin_);
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPUOnViewportRenderingInternal(params->context, params->world_binding);
  }
}

void PlaneImpl::GPUCreatePlaneInternal() {
  gpu_.batch = renderer::QuadBatch::Make(**context()->render_device);
  gpu_.shader_binding =
      context()->render.pipeline_loader->viewport.CreateBinding();

  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::Binding_Flat::Params),
      "plane.flat.uniform", &gpu_.uniform_buffer, Diligent::USAGE_DEFAULT);
}

void PlaneImpl::GPUUpdatePlaneQuadArrayInternal(
    Diligent::IDeviceContext* render_context,
    const base::Rect& src_rect,
    const base::Vec2i& viewport_size,
    const base::Vec2& scale,
    const base::Vec2i& origin) {
  // Source texture
  auto* texture = **bitmap_;

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
  const float wrap_ox = value_wrap(static_cast<float>(origin.x), item_x);
  const float wrap_oy = value_wrap(static_cast<float>(origin.y), item_y);

  // Optimized tile calculation using simplified formula
  const float total_x = static_cast<float>(viewport_size.x) + wrap_ox;
  const float total_y = static_cast<float>(viewport_size.y) + wrap_oy;
  const int32_t tile_x = static_cast<int32_t>(std::ceil(total_x / item_x));
  const int32_t tile_y = static_cast<int32_t>(std::ceil(total_y / item_y));

  // Prepare vertex buffer
  const int32_t quad_size = tile_x * tile_y;
  gpu_.cache.resize(quad_size);
  const base::Vec4 opacity_norm(static_cast<float>(opacity_) / 255.0f);

  // Pointer-based vertex writing with accumulative positioning
  renderer::Quad* quad_ptr = gpu_.cache.data();
  float current_y = -wrap_oy;
  for (int32_t y = 0; y < tile_y; ++y) {
    float current_x = -wrap_ox;
    for (int32_t x = 0; x < tile_x; ++x) {
      // Set vertex properties directly through pointer
      const base::RectF pos(current_x, current_y, item_x, item_y);
      renderer::Quad::SetPositionRect(quad_ptr, pos);
      renderer::Quad::SetTexCoordRect(quad_ptr, src_rect,
                                      texture->size.Recast<float>());
      renderer::Quad::SetColor(quad_ptr, opacity_norm);

      // Move to next quad using pointer arithmetic
      ++quad_ptr;
      current_x += item_x;  // X-axis accumulation
    }
    current_y += item_y;  // Y-axis accumulation
  }

  context()->render.quad_index->Allocate(quad_size);
  gpu_.quad_size = quad_size;
  gpu_.batch.QueueWrite(render_context, gpu_.cache.data(), gpu_.cache.size());

  renderer::Binding_Flat::Params transient_uniform;
  transient_uniform.Color = color_->AsNormColor();
  transient_uniform.Tone = tone_->AsNormColor();
  render_context->UpdateBuffer(
      gpu_.uniform_buffer, 0, sizeof(transient_uniform), &transient_uniform,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void PlaneImpl::GPUOnViewportRenderingInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::IBuffer* world_binding) {
  // Source texture
  auto* texture = **bitmap_;

  // Render device etc
  auto* pipeline =
      context()->render.pipeline_states->viewport[blend_type_].RawPtr();

  // Setup uniform params
  gpu_.shader_binding.u_transform->Set(world_binding);
  gpu_.shader_binding.u_texture->Set(texture->shader_resource_view);
  gpu_.shader_binding.u_params->Set(gpu_.uniform_buffer);

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *gpu_.shader_binding,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *gpu_.batch;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render.quad_index, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6 * gpu_.cache.size();
  draw_indexed_attribs.IndexType = context()->render.quad_index->GetIndexType();
  render_context->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content
