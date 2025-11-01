// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/sprite_impl.h"

#include "content/context/execution_context.h"
#include "content/screen/renderscreen_impl.h"
#include "content/screen/viewport_impl.h"

namespace content {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int32_t kWaveBlockAlign = 8;

inline float DegreesToRadians(float degrees) {
  return degrees * (kPi / 180.0f);
}

}  // namespace

// static
scoped_refptr<Sprite> Sprite::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  bool disable_vertical_sort,
                                  ExceptionState& exception_state) {
  return base::MakeRefCounted<SpriteImpl>(
      execution_context, ViewportImpl::From(viewport), disable_vertical_sort);
}

///////////////////////////////////////////////////////////////////////////////
// SpriteImpl Implement

SpriteImpl::SpriteImpl(ExecutionContext* execution_context,
                       scoped_refptr<ViewportImpl> parent,
                       bool disable_vertical_sort)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()),
      rgss2_style_(execution_context->engine_profile->api_version >=
                   ContentProfile::APIVersion::RGSS2),
      disable_vertical_sort_(disable_vertical_sort),
      viewport_(parent),
      src_rect_(base::MakeRefCounted<RectImpl>(base::Rect())),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      tone_(base::MakeRefCounted<ToneImpl>(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &SpriteImpl::DrawableNodeHandlerInternal, base::Unretained(this)));
  node_.SetupBatchable(this);

  src_rect_observer_ = src_rect_->AddObserver(base::BindRepeating(
      &SpriteImpl::SrcRectChangedInternal, base::Unretained(this)));

  std::memset(&uniform_params_, 0, sizeof(uniform_params_));
  uniform_params_.Scale = base::Vec4(1.0f);

  GPUCreateSpriteInternal();
}

DISPOSABLE_DEFINITION(SpriteImpl);

void SpriteImpl::Flash(scoped_refptr<Color> color,
                       uint32_t duration,
                       ExceptionState& exception_state) {
  DISPOSE_CHECK;

  std::optional<base::Vec4> flash_color = std::nullopt;
  if (color)
    flash_color = ColorImpl::From(color)->AsNormColor();
  flash_emitter_.Setup(flash_color, duration);
}

void SpriteImpl::Update(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  flash_emitter_.Update();

  if (wave_.amp) {
    wave_.phase += wave_.speed / 180.0f;
    wave_.dirty = true;
  }
}

uint32_t SpriteImpl::Width(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return src_rect_->Get_Width(exception_state);
}

uint32_t SpriteImpl::Height(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return src_rect_->Get_Height(exception_state);
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Bitmap,
    scoped_refptr<Bitmap>,
    SpriteImpl,
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
    SpriteImpl,
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
    SpriteImpl,
    { return viewport_; },
    {
      DISPOSE_CHECK;
      if (viewport_ != value) {
        viewport_ = ViewportImpl::From(value);
        node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                         : context()->screen_drawable_node);
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Visible,
    bool,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return node_.GetVisibility();
    },
    {
      DISPOSE_CHECK;
      node_.SetNodeVisibility(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    X,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return uniform_params_.Position.x;
    },
    {
      DISPOSE_CHECK;
      uniform_params_.Position.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Y,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return uniform_params_.Position.y;
    },
    {
      DISPOSE_CHECK;
      // Set normal Y attribute for all version
      uniform_params_.Position.y = value;
      // Sort with Z and Y attribute on RGSS 2/3
      if (rgss2_style_ && !disable_vertical_sort_) {
        int64_t current_order = node_.GetSortKeys()->weight[0];
        node_.SetNodeSortWeight(current_order, value);
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Z,
    int32_t,
    SpriteImpl,
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
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return uniform_params_.Origin.x;
    },
    {
      DISPOSE_CHECK;
      uniform_params_.Origin.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Oy,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return uniform_params_.Origin.y;
    },
    {
      DISPOSE_CHECK;
      uniform_params_.Origin.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    ZoomX,
    float,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);
      return uniform_params_.Scale.x;
    },
    {
      DISPOSE_CHECK;
      uniform_params_.Scale.x = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    ZoomY,
    float,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);
      return uniform_params_.Scale.y;
    },
    {
      DISPOSE_CHECK;
      uniform_params_.Scale.y = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Angle,
    float,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);
      return angle_;
    },
    {
      DISPOSE_CHECK;
      angle_ = value;
      uniform_params_.Rotation.x = angle_ * kPi / 180.0f;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    WaveAmp,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return wave_.amp;
    },
    {
      DISPOSE_CHECK;
      wave_.amp = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    WaveLength,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return wave_.length;
    },
    {
      DISPOSE_CHECK;
      wave_.length = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    WaveSpeed,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return wave_.speed;
    },
    {
      DISPOSE_CHECK;
      wave_.speed = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    WavePhase,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return wave_.phase;
    },
    {
      DISPOSE_CHECK;
      wave_.phase = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Mirror,
    bool,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(false);
      return mirror_;
    },
    {
      DISPOSE_CHECK;
      mirror_ = value;
      src_rect_dirty_ = true;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    BushDepth,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bush_.depth;
    },
    {
      DISPOSE_CHECK;
      bush_.depth = value;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    BushOpacity,
    int32_t,
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(0);
      return bush_.opacity;
    },
    {
      DISPOSE_CHECK;
      bush_.opacity = std::clamp(value, 0, 255);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Opacity,
    int32_t,
    SpriteImpl,
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
    SpriteImpl,
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
    SpriteImpl,
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
    SpriteImpl,
    {
      DISPOSE_CHECK_RETURN(nullptr);
      return tone_;
    },
    {
      DISPOSE_CHECK;
      CHECK_ATTRIBUTE_VALUE;
      *tone_ = *ToneImpl::From(value);
    });

void SpriteImpl::OnObjectDisposed() {
  node_.DisposeNode();

  GPUData empty_data;
  std::swap(gpu_, empty_data);
}

void SpriteImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  BitmapTexture* current_texture =
      Disposable::IsValid(bitmap_.get()) ? **bitmap_ : nullptr;
  if (!current_texture)
    return;

  if (flash_emitter_.IsFlashing() && flash_emitter_.IsInvalid())
    return;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    // Uniform effect parameters
    base::Vec4 composite_color = color_->AsNormColor();
    base::Vec4 flash_color = flash_emitter_.GetColor();
    base::Vec4 target_color = composite_color;
    if (flash_emitter_.IsFlashing())
      target_color =
          (flash_color.w > composite_color.w ? flash_color : composite_color);

    // Make sprite uniform params
    base::Rect src_rect = src_rect_->AsBaseRect();
    uniform_params_.Color = target_color;
    uniform_params_.Tone = tone_->AsNormColor();
    uniform_params_.Opacity.x = static_cast<float>(opacity_) / 255.0f;
    uniform_params_.BushDepthAndOpacity.x =
        static_cast<float>(src_rect.y + src_rect.height - bush_.depth) /
        current_texture->size.y;
    uniform_params_.BushDepthAndOpacity.y =
        static_cast<float>(bush_.opacity) / 255.0f;

    // Sprite batch test
    DrawableNode* next_node = node_.GetNextNode();
    SpriteImpl* next_sprite =
        next_node ? next_node->CastToNode<SpriteImpl>() : nullptr;
    BitmapTexture* next_texture =
        GetOtherRenderBatchableTextureInternal(next_sprite);

    GPUUpdateBatchSpriteInternal(params->context, current_texture, next_texture,
                                 src_rect);

    wave_.dirty = false;
    src_rect_dirty_ = false;
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    GPUOnSpriteRenderingInternal(params->context, params->world_binding,
                                 current_texture);
  }
}

void SpriteImpl::SrcRectChangedInternal() {
  src_rect_dirty_ = true;
}

BitmapTexture* SpriteImpl::GetOtherRenderBatchableTextureInternal(
    SpriteImpl* other) {
  // Disable batch if other is not a sprite
  if (!other)
    return nullptr;

  // Disable batch if other invisible
  if (!other->node_.GetVisibility())
    return nullptr;

  // Disable batch if different blend effect
  if (other->blend_type_ != blend_type_)
    return nullptr;

  // Disable batch if other has not an invalid texture
  if (!other->bitmap_)
    return nullptr;

  // Disable batch if flash effect
  if (other->flash_emitter_.IsFlashing() && other->flash_emitter_.IsInvalid())
    return nullptr;

  return **other->bitmap_;
}

void SpriteImpl::GPUCreateSpriteInternal() {
  const bool enable_batch = context()->sprite_batcher->IsBatchEnabled();
  if (!enable_batch)
    Diligent::CreateUniformBuffer(
        **context()->render_device, sizeof(renderer::Binding_Sprite::Params),
        "sprite.non-batch.uniform", &gpu_.single_uniform,
        Diligent::USAGE_DEFAULT, Diligent::BIND_UNIFORM_BUFFER,
        Diligent::CPU_ACCESS_NONE);
}

void SpriteImpl::GPUUpdateWaveSpriteInternal(BitmapTexture* texture,
                                             const base::Rect& src_rect) {
  int32_t last_block_aligned_size = src_rect.height % kWaveBlockAlign;
  int32_t loop_block = src_rect.height / kWaveBlockAlign;
  int32_t block_count = loop_block + !!last_block_aligned_size;
  float wave_phase = DegreesToRadians(wave_.phase);

  gpu_.wave_cache.resize(block_count);
  renderer::Quad* quad = gpu_.wave_cache.data();

  auto emit_wave_block = [&](int32_t block_y, int32_t block_size) {
    float wave_offset =
        wave_phase + (static_cast<float>(block_y) / wave_.length) * kPi;
    float block_x = std::sin(wave_offset) * wave_.amp;

    base::Rect tex(src_rect.x, src_rect.y + block_y, src_rect.width,
                   block_size);
    base::Rect pos(base::Vec2i(block_x, block_y), tex.Size());

    if (mirror_) {
      tex.x += tex.width;
      tex.width = -tex.width;
    }

    renderer::Quad::SetPositionRect(quad, pos);
    renderer::Quad::SetTexCoordRect(quad, tex, texture->size.Recast<float>());
    ++quad;
  };

  for (int32_t i = 0; i < loop_block; ++i)
    emit_wave_block(i * kWaveBlockAlign, kWaveBlockAlign);

  if (last_block_aligned_size)
    emit_wave_block(loop_block * kWaveBlockAlign, last_block_aligned_size);
}

void SpriteImpl::GPUUpdateBatchSpriteInternal(
    Diligent::IDeviceContext* render_context,
    BitmapTexture* texture,
    BitmapTexture* next_texture,
    const base::Rect& src_rect) {
  // Update sprite quad if need
  if (wave_.amp) {
    // Wave process if need
    GPUUpdateWaveSpriteInternal(texture, src_rect);
  } else if (src_rect_dirty_) {
    base::Rect rect = src_rect;

    rect.width = std::clamp(rect.width, 0, texture->size.x - rect.x);
    rect.height = std::clamp(rect.height, 0, texture->size.y - rect.y);

    base::Rect texcoord = rect;
    if (mirror_)
      texcoord =
          base::Rect(rect.x + rect.width, rect.y, -rect.width, rect.height);

    renderer::Quad::SetPositionRect(&gpu_.quad, rect.Size().Recast<float>());
    renderer::Quad::SetTexCoordRect(&gpu_.quad, texcoord,
                                    texture->size.Recast<float>());
  }

  // Start a batch if no context
  if (!context()->sprite_batcher->GetCurrentTexture())
    context()->sprite_batcher->BeginBatch(texture);

  // Push this node into batch scheduler if batchable
  if (texture == context()->sprite_batcher->GetCurrentTexture()) {
    if (wave_.amp) {
      for (const auto& it : gpu_.wave_cache)
        context()->sprite_batcher->PushSprite(it, uniform_params_);
    } else {
      context()->sprite_batcher->PushSprite(gpu_.quad, uniform_params_);
    }
  }

  // Determine batch mode
  const bool enable_batch = context()->sprite_batcher->IsBatchEnabled();

  // End batch on this node if next cannot be batchable
  if (!enable_batch || (context()->sprite_batcher->GetCurrentTexture() &&
                        next_texture != texture)) {
    // Execute batch draw info on this node
    context()->sprite_batcher->EndBatch(&gpu_.instance_offset,
                                        &gpu_.instance_count);
  } else {
    // Do not draw quads on current node
    gpu_.instance_count = 0;
  }

  // Update non batch uniform
  if (!enable_batch)
    render_context->UpdateBuffer(
        gpu_.single_uniform, 0, sizeof(uniform_params_), &uniform_params_,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void SpriteImpl::GPUOnSpriteRenderingInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::IBuffer* world_binding,
    BitmapTexture* texture) {
  if (gpu_.instance_count) {
    // Batch draw
    auto* pipeline =
        context()->render.pipeline_states->sprite[blend_type_].RawPtr();

    // Determine batch mode
    const bool enable_batch = context()->sprite_batcher->IsBatchEnabled();

    // Setup uniform params
    context()->sprite_batcher->GetShaderBinding().u_transform->Set(
        world_binding);
    context()->sprite_batcher->GetShaderBinding().u_texture->Set(
        texture->shader_resource_view);
    if (enable_batch) {
      context()->sprite_batcher->GetShaderBinding().u_params->Set(
          context()->sprite_batcher->GetUniformBinding());
    } else {
      context()->sprite_batcher->GetShaderBinding().u_effect->Set(
          gpu_.single_uniform);
    }

    // Apply pipeline state
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *context()->sprite_batcher->GetShaderBinding(),
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer =
        context()->sprite_batcher->GetVertexBuffer();
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render.quad_index, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    if (enable_batch) {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6 * gpu_.instance_count;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = 6 * gpu_.instance_offset;
      render_context->DrawIndexed(draw_indexed_attribs);
    } else {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType =
          context()->render.quad_index->GetIndexType();
      draw_indexed_attribs.FirstIndexLocation = 6 * gpu_.instance_offset;
      render_context->DrawIndexed(draw_indexed_attribs);
    }
  }
}

}  // namespace content
