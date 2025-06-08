// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/sprite_impl.h"

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

scoped_refptr<Sprite> Sprite::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  ExceptionState& exception_state) {
  return base::MakeRefCounted<SpriteImpl>(execution_context,
                                          ViewportImpl::From(viewport));
}

SpriteImpl::SpriteImpl(ExecutionContext* execution_context,
                       scoped_refptr<ViewportImpl> parent)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()),
      rgss2_style_(execution_context->engine_profile->sprite_vertical_sort &&
                   execution_context->engine_profile->api_version >=
                       ContentProfile::APIVersion::RGSS2),
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
}

SpriteImpl::~SpriteImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void SpriteImpl::SetLabel(const base::String& label,
                          ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

void SpriteImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool SpriteImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void SpriteImpl::Flash(scoped_refptr<Color> color,
                       uint32_t duration,
                       ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  std::optional<base::Vec4> flash_color = std::nullopt;
  if (color)
    flash_color = ColorImpl::From(color)->AsNormColor();
  flash_emitter_.Setup(flash_color, duration);
}

void SpriteImpl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  flash_emitter_.Update();

  if (wave_.amp) {
    wave_.phase += wave_.speed / 180.0f;
    wave_.dirty = true;
  }
}

uint32_t SpriteImpl::Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return src_rect_->Get_Width(exception_state);
}

uint32_t SpriteImpl::Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return src_rect_->Get_Height(exception_state);
}

scoped_refptr<Bitmap> SpriteImpl::Get_Bitmap(ExceptionState& exception_state) {
  return bitmap_;
}

void SpriteImpl::Put_Bitmap(const scoped_refptr<Bitmap>& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bitmap_ = CanvasImpl::FromBitmap(value);
  if (bitmap_)
    src_rect_->SetBase(bitmap_->GetAgent()->size);
}

scoped_refptr<Rect> SpriteImpl::Get_SrcRect(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return src_rect_;
}

void SpriteImpl::Put_SrcRect(const scoped_refptr<Rect>& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *src_rect_ = *RectImpl::From(value);
}

scoped_refptr<Viewport> SpriteImpl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void SpriteImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

bool SpriteImpl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return node_.GetVisibility();
}

void SpriteImpl::Put_Visible(const bool& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeVisibility(value);
}

int32_t SpriteImpl::Get_X(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Position.x;
}

void SpriteImpl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.Position.x = value;
}

int32_t SpriteImpl::Get_Y(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Position.y;
}

void SpriteImpl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  // Set normal Y attribute for all version.
  uniform_params_.Position.y = value;

  // Sort with Z and Y attribute on RGSS 2/3.
  if (rgss2_style_) {
    int64_t current_order = node_.GetSortKeys()->weight[0];
    node_.SetNodeSortWeight(current_order, value);
  }
}

int32_t SpriteImpl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return node_.GetSortKeys()->weight[0];
}

void SpriteImpl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeSortWeight(value);
}

int32_t SpriteImpl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Origin.x;
}

void SpriteImpl::Put_Ox(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.Origin.x = value;
}

int32_t SpriteImpl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Origin.y;
}

void SpriteImpl::Put_Oy(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.Origin.y = value;
}

float SpriteImpl::Get_ZoomX(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Scale.x;
}

void SpriteImpl::Put_ZoomX(const float& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.Scale.x = value;
}

float SpriteImpl::Get_ZoomY(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.Scale.y;
}

void SpriteImpl::Put_ZoomY(const float& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.Scale.y = value;
}

float SpriteImpl::Get_Angle(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return angle_;
}

void SpriteImpl::Put_Angle(const float& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  angle_ = value;
  uniform_params_.Rotation = angle_ * kPi / 180.0f;
}

int32_t SpriteImpl::Get_WaveAmp(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return wave_.amp;
}

void SpriteImpl::Put_WaveAmp(const int32_t& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  wave_.amp = value;
}

int32_t SpriteImpl::Get_WaveLength(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return wave_.length;
}

void SpriteImpl::Put_WaveLength(const int32_t& value,
                                ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  wave_.length = value;
}

int32_t SpriteImpl::Get_WaveSpeed(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return wave_.speed;
}

void SpriteImpl::Put_WaveSpeed(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  wave_.speed = value;
}

int32_t SpriteImpl::Get_WavePhase(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return wave_.phase;
}

void SpriteImpl::Put_WavePhase(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  wave_.phase = value;
}

bool SpriteImpl::Get_Mirror(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return mirror_;
}

void SpriteImpl::Put_Mirror(const bool& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  mirror_ = value;
  src_rect_dirty_ = true;
}

int32_t SpriteImpl::Get_BushDepth(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bush_.depth;
}

void SpriteImpl::Put_BushDepth(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bush_.depth = value;
}

int32_t SpriteImpl::Get_BushOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bush_.opacity;
}

void SpriteImpl::Put_BushOpacity(const int32_t& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bush_.opacity = std::clamp(value, 0, 255);
}

int32_t SpriteImpl::Get_Opacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return opacity_;
}

void SpriteImpl::Put_Opacity(const int32_t& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t SpriteImpl::Get_BlendType(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return blend_type_;
}

void SpriteImpl::Put_BlendType(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  blend_type_ = value;
}

scoped_refptr<Color> SpriteImpl::Get_Color(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return color_;
}

void SpriteImpl::Put_Color(const scoped_refptr<Color>& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *color_ = *ColorImpl::From(value);
}

scoped_refptr<Tone> SpriteImpl::Get_Tone(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return tone_;
}

void SpriteImpl::Put_Tone(const scoped_refptr<Tone>& value,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CHECK_ATTRIBUTE_VALUE;

  *tone_ = *ToneImpl::From(value);
}

void SpriteImpl::OnObjectDisposed() {
  node_.DisposeNode();

  Agent empty_agent;
  std::swap(agent_, empty_agent);
}

void SpriteImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  CanvasImpl::Agent* current_texture = bitmap_ ? bitmap_->GetAgent() : nullptr;
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
    uniform_params_.Opacity = static_cast<float>(opacity_) / 255.0f;
    uniform_params_.BushDepthAndOpacity.x =
        static_cast<float>(src_rect.y + src_rect.height - bush_.depth) /
        current_texture->size.y;
    uniform_params_.BushDepthAndOpacity.y =
        static_cast<float>(bush_.opacity) / 255.0f;

    // Sprite batch test
    DrawableNode* next_node = node_.GetNextNode();
    SpriteImpl* next_sprite =
        next_node ? next_node->CastToNode<SpriteImpl>() : nullptr;
    CanvasImpl::Agent* next_texture =
        GetOtherRenderBatchableTextureInternal(next_sprite);

    GPUUpdateBatchSpriteInternal(current_texture, next_texture, src_rect);

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

CanvasImpl::Agent* SpriteImpl::GetOtherRenderBatchableTextureInternal(
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

  return other->bitmap_->GetAgent();
}

void SpriteImpl::GPUUpdateWaveSpriteInternal(CanvasImpl::Agent* texture,
                                             const base::Rect& src_rect) {
  int32_t last_block_aligned_size = src_rect.height % kWaveBlockAlign;
  int32_t loop_block = src_rect.height / kWaveBlockAlign;
  int32_t block_count = loop_block + !!last_block_aligned_size;
  float wave_phase = DegreesToRadians(wave_.phase);

  agent_.wave_cache.resize(block_count);
  renderer::Quad* quad = agent_.wave_cache.data();

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
    renderer::Quad::SetTexCoordRect(quad, tex, texture->size);
    ++quad;
  };

  for (int32_t i = 0; i < loop_block; ++i)
    emit_wave_block(i * kWaveBlockAlign, kWaveBlockAlign);

  if (last_block_aligned_size)
    emit_wave_block(loop_block * kWaveBlockAlign, last_block_aligned_size);
}

void SpriteImpl::GPUUpdateBatchSpriteInternal(CanvasImpl::Agent* texture,
                                              CanvasImpl::Agent* next_texture,
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

    renderer::Quad::SetPositionRect(&agent_.quad, base::Vec2(rect.Size()));
    renderer::Quad::SetTexCoordRect(&agent_.quad, texcoord, texture->size);
  }

  // Start a batch if no context
  if (!context()->sprite_batcher->GetCurrentTexture())
    context()->sprite_batcher->BeginBatch(texture);

  // Push this node into batch scheduler if batchable
  if (texture == context()->sprite_batcher->GetCurrentTexture()) {
    if (wave_.amp) {
      for (const auto& it : agent_.wave_cache)
        context()->sprite_batcher->PushSprite(it, uniform_params_);
    } else {
      context()->sprite_batcher->PushSprite(agent_.quad, uniform_params_);
    }
  }

  // Determine batch mode
  const bool enable_batch = context()->sprite_batcher->IsBatchEnabled();

  // End batch on this node if next cannot be batchable
  if (!enable_batch || (context()->sprite_batcher->GetCurrentTexture() &&
                        next_texture != texture)) {
    // Execute batch draw info on this node
    context()->sprite_batcher->EndBatch(&agent_.instance_offset,
                                        &agent_.instance_count);
  } else {
    // Do not draw quads on current node
    agent_.instance_count = 0;
  }
}

void SpriteImpl::GPUOnSpriteRenderingInternal(
    renderer::RenderContext* render_context,
    Diligent::IBuffer* world_binding,
    CanvasImpl::Agent* texture) {
  if (agent_.instance_count) {
    // Batch draw
    auto& pipeline_set = context()->render_device->GetPipelines()->sprite;
    auto* pipeline =
        pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type_));

    // Setup uniform params
    context()->sprite_batcher->GetShaderBinding().u_transform->Set(
        world_binding);
    context()->sprite_batcher->GetShaderBinding().u_texture->Set(
        texture->resource);
    context()->sprite_batcher->GetShaderBinding().u_params->Set(
        context()->sprite_batcher->GetUniformBinding());

    // Apply pipeline state
    (*render_context)->SetPipelineState(pipeline);
    (*render_context)
        ->CommitShaderResources(
            *context()->sprite_batcher->GetShaderBinding(),
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Determine batch mode
    const bool enable_batch = context()->sprite_batcher->IsBatchEnabled();

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer[] = {
        context()->sprite_batcher->GetVertexBuffer(),
        context()->sprite_batcher->GetInstanceBuffer()};
    (*render_context)
        ->SetVertexBuffers(0, 1 + !enable_batch, vertex_buffer, nullptr,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (*render_context)
        ->SetIndexBuffer(**context()->render_device->GetQuadIndex(), 0,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    if (enable_batch) {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6 * agent_.instance_count;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = 6 * agent_.instance_offset;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
    } else {
      Diligent::DrawIndexedAttribs draw_indexed_attribs;
      draw_indexed_attribs.NumIndices = 6;
      draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
      draw_indexed_attribs.FirstIndexLocation = 6 * agent_.instance_offset;
      draw_indexed_attribs.NumInstances = 1;
      draw_indexed_attribs.FirstInstanceLocation = agent_.instance_offset;
      (*render_context)->DrawIndexed(draw_indexed_attribs);
    }
  }
}

}  // namespace content
