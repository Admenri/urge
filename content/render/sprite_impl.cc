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

void GPUCreateSpriteInternal(renderer::RenderDevice* device,
                             SpriteAgent* agent) {
  agent->wave_cache.reserve(1 << 3);

  if (!device->GetPipelines()->sprite.storage_buffer_support) {
    agent->single_binding = device->GetPipelines()->sprite.CreateBinding();
    agent->single_vertex = renderer::QuadBatch::Make(**device, 1);
    Diligent::CreateUniformBuffer(
        **device, sizeof(renderer::Binding_Sprite::Params),
        "sprite.single.uniform", &agent->single_uniform,
        Diligent::USAGE_DEFAULT);
  }
}

void GPUDestroySpriteInternal(SpriteAgent* agent) {
  delete agent;
}

void GPUUpdateWaveSpriteInternal(
    SpriteBatch* batch_scheduler,
    SpriteAgent* agent,
    TextureAgent* texture,
    const base::Rect& src_rect,
    const SpriteImpl::WaveParams& wave,
    const renderer::Binding_Sprite::Params& uniform,
    bool mirror) {
  int32_t last_block_aligned_size = src_rect.height % kWaveBlockAlign;
  int32_t loop_block = src_rect.height / kWaveBlockAlign;
  int32_t block_count = loop_block + !!last_block_aligned_size;
  float wave_phase = DegreesToRadians(wave.phase);

  agent->wave_cache.resize(block_count);
  renderer::Quad* quad = agent->wave_cache.data();

  auto emit_wave_block = [&](int32_t block_y, int32_t block_size) {
    float wave_offset =
        wave_phase + (static_cast<float>(block_y) / wave.length) * kPi;
    float block_x = std::sin(wave_offset) * wave.amp;

    base::Rect tex(src_rect.x, src_rect.y + block_y, src_rect.width,
                   block_size);
    base::Rect pos(base::Vec2i(block_x, block_y), tex.Size());

    if (mirror) {
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

void GPUUpdateBatchSpriteInternal(
    renderer::RenderDevice* device,
    renderer::RenderContext* context,
    SpriteBatch* batch_scheduler,
    SpriteAgent* agent,
    TextureAgent* texture,
    TextureAgent* next_texture,
    const SpriteImpl::WaveParams& wave,
    const renderer::Binding_Sprite::Params& uniform,
    const base::Rect& src_rect,
    int32_t mirror,
    bool src_rect_dirty) {
  // Update sprite quad if need
  if (wave.amp) {
    // Wave process if need
    GPUUpdateWaveSpriteInternal(batch_scheduler, agent, texture, src_rect, wave,
                                uniform, mirror);
  } else if (src_rect_dirty) {
    base::Rect rect = src_rect;

    rect.width = std::clamp(rect.width, 0, texture->size.x - rect.x);
    rect.height = std::clamp(rect.height, 0, texture->size.y - rect.y);

    base::Rect texcoord = rect;
    if (mirror)
      texcoord =
          base::Rect(rect.x + rect.width, rect.y, -rect.width, rect.height);

    renderer::Quad::SetPositionRect(&agent->quad, base::Vec2(rect.Size()));
    renderer::Quad::SetTexCoordRect(&agent->quad, texcoord, texture->size);
  }

  if (!agent->single_binding) {
    // Start a batch if no context
    if (!batch_scheduler->GetCurrentTexture())
      batch_scheduler->BeginBatch(texture);

    // Push this node into batch scheduler if batchable
    if (texture == batch_scheduler->GetCurrentTexture()) {
      if (wave.amp) {
        for (const auto& it : agent->wave_cache)
          batch_scheduler->PushSprite(it, uniform);
      } else {
        batch_scheduler->PushSprite(agent->quad, uniform);
      }
    }

    // End batch on this node if next cannot be batchable
    if (batch_scheduler->GetCurrentTexture() && next_texture != texture) {
      // Execute batch draw info on this node
      batch_scheduler->EndBatch(&agent->instance_offset,
                                &agent->instance_count);
    } else {
      // Do not draw quads on current node
      agent->instance_count = 0;
    }
  } else {
    if (wave.amp) {
      agent->single_wave_quad_count = agent->wave_cache.size();
      device->GetQuadIndex()->Allocate(agent->single_wave_quad_count);

      agent->single_vertex->QueueWrite(**context, agent->wave_cache.data(),
                                       agent->wave_cache.size());
    } else {
      agent->single_wave_quad_count = 1;
      agent->single_vertex->QueueWrite(**context, &agent->quad);
    }

    (*context)->UpdateBuffer(
        agent->single_uniform, 0, sizeof(uniform), &uniform,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
}

void GPUOnSpriteRenderingInternal(renderer::RenderDevice* device,
                                  renderer::RenderContext* context,
                                  SpriteBatch* batch_scheduler,
                                  Diligent::IBuffer** world_binding,
                                  SpriteAgent* agent,
                                  TextureAgent* texture,
                                  int32_t blend_type,
                                  bool wave_active) {
  if (agent->single_binding) {
    // Single drawcall
    auto& pipeline_set = device->GetPipelines()->sprite;
    auto* pipeline =
        pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type));

    // Setup uniform params
    agent->single_binding->u_transform->Set(*world_binding);
    agent->single_binding->u_texture->Set(texture->view);
    agent->single_binding->u_param->Set(agent->single_uniform);

    // Apply pipeline state
    (*context)->SetPipelineState(pipeline);
    (*context)->CommitShaderResources(
        **agent->single_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = **agent->single_vertex;
    (*context)->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (*context)->SetIndexBuffer(
        **device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6 * agent->single_wave_quad_count;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    (*context)->DrawIndexed(draw_indexed_attribs);

    return;
  }

  if (agent->instance_count) {
    // Batch draw
    auto& pipeline_set = device->GetPipelines()->sprite;
    auto* pipeline =
        pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type));

    // Setup uniform params
    batch_scheduler->GetShaderBinding()->u_transform->Set(*world_binding);
    batch_scheduler->GetShaderBinding()->u_texture->Set(texture->view);
    batch_scheduler->GetShaderBinding()->u_params->Set(
        batch_scheduler->GetUniformBinding());

    // Apply pipeline state
    (*context)->SetPipelineState(pipeline);
    (*context)->CommitShaderResources(
        **batch_scheduler->GetShaderBinding(),
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = batch_scheduler->GetVertexBuffer();
    (*context)->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (*context)->SetIndexBuffer(
        **device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6 * agent->instance_count;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    draw_indexed_attribs.FirstIndexLocation = 6 * agent->instance_offset;
    (*context)->DrawIndexed(draw_indexed_attribs);
  }
}

}  // namespace

scoped_refptr<Sprite> Sprite::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  ExceptionState& exception_state) {
  return base::MakeRefCounted<SpriteImpl>(execution_context->graphics,
                                          ViewportImpl::From(viewport));
}

SpriteImpl::SpriteImpl(RenderScreenImpl* screen,
                       scoped_refptr<ViewportImpl> parent)
    : GraphicsChild(screen),
      Disposable(screen),
      node_(parent ? parent->GetDrawableController()
                   : screen->GetDrawableController(),
            SortKey()),
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

  agent_ = new SpriteAgent;
  screen->PostTask(
      base::BindOnce(&GPUCreateSpriteInternal, screen->GetDevice(), agent_));
}

SpriteImpl::~SpriteImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void SpriteImpl::SetLabel(const std::string& label,
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
    src_rect_->SetBase(bitmap_->AsBaseSize());
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
                                   : screen()->GetDrawableController());
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
  if (screen()->GetAPIVersion() >= ContentProfile::APIVersion::RGSS2) {
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

  screen()->PostTask(base::BindOnce(&GPUDestroySpriteInternal, agent_));
  agent_ = nullptr;
}

void SpriteImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  TextureAgent* current_texture = bitmap_ ? bitmap_->GetAgent() : nullptr;
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
    TextureAgent* next_texture =
        GetOtherRenderBatchableTextureInternal(next_sprite);

    screen()->PostTask(base::BindOnce(
        &GPUUpdateBatchSpriteInternal, params->device, params->context,
        screen()->GetSpriteBatch(), agent_, current_texture, next_texture,
        wave_, uniform_params_, src_rect, mirror_, src_rect_dirty_));

    wave_.dirty = false;
    src_rect_dirty_ = false;
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    screen()->PostTask(base::BindOnce(
        &GPUOnSpriteRenderingInternal, params->device, params->context,
        screen()->GetSpriteBatch(), params->world_binding, agent_,
        current_texture, blend_type_, !!wave_.amp));
  }
}

void SpriteImpl::SrcRectChangedInternal() {
  src_rect_dirty_ = true;
}

TextureAgent* SpriteImpl::GetOtherRenderBatchableTextureInternal(
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

}  // namespace content
