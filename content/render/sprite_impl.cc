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
  agent->vertex_buffer = renderer::CreateQuadBuffer(
      **device, "sprite.vertex",
      wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex);

  agent->uniform_buffer =
      renderer::CreateUniformBuffer<renderer::SpriteUniform>(
          **device, "sprite.uniform", wgpu::BufferUsage::CopyDst);
  agent->uniform_binding =
      renderer::SpriteUniform::CreateGroup(**device, agent->uniform_buffer);
}

void GPUDestroySpriteInternal(SpriteAgent* agent) {
  delete agent;
}

void GPUUpdateSpriteWaveInternal(renderer::RenderDevice* device,
                                 SpriteAgent* agent,
                                 wgpu::CommandEncoder* encoder,
                                 const base::Vec2& position,
                                 const base::Rect& src_rect,
                                 const SpriteImpl::WaveParams& wave,
                                 bool mirror) {
  if (!wave.amp || !wave.dirty)
    return;

  int32_t last_block_aligned_size = src_rect.height % kWaveBlockAlign;
  int32_t loop_block = src_rect.height / kWaveBlockAlign;
  int32_t block_count = loop_block + !!last_block_aligned_size;
  float wave_phase = DegreesToRadians(wave.phase);

  agent->wave_index_count = block_count * 6;
  agent->wave_cache.resize(block_count);
  renderer::Quad* quad = agent->wave_cache.data();

  auto emit_wave_block = [&](int block_y, int block_size) {
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
    renderer::Quad::SetTexCoordRect(quad, tex);
    ++quad;
  };

  for (int i = 0; i < loop_block; ++i)
    emit_wave_block(i * kWaveBlockAlign, kWaveBlockAlign);

  if (last_block_aligned_size)
    emit_wave_block(loop_block * kWaveBlockAlign, last_block_aligned_size);

  if (!agent->wave_batch)
    agent->wave_batch =
        renderer::QuadBatch::Make(**device, agent->wave_cache.size());
  agent->wave_batch->QueueWrite(*encoder, agent->wave_cache.data(),
                                agent->wave_cache.size());
}

void GPUUpdateSpriteVerticesInternal(renderer::RenderDevice* device,
                                     wgpu::CommandEncoder* encoder,
                                     SpriteAgent* agent,
                                     TextureAgent* texture,
                                     const renderer::SpriteUniform& uniform,
                                     const base::Rect& src_rect,
                                     int32_t opacity,
                                     int32_t mirror,
                                     const SpriteImpl::WaveParams& wave,
                                     const base::Vec2& position,
                                     bool src_rect_dirty) {
  GPUUpdateSpriteWaveInternal(device, agent, encoder, position, src_rect, wave,
                              mirror);

  if (texture && src_rect_dirty) {
    auto bitmap_size = texture->size;
    auto rect = src_rect;

    rect.width = std::clamp(rect.width, 0, bitmap_size.x - rect.x);
    rect.height = std::clamp(rect.height, 0, bitmap_size.y - rect.y);

    renderer::Quad transient_quad;
    renderer::Quad::SetColor(&transient_quad, base::Vec4(opacity / 255.0f));
    renderer::Quad::SetPositionRect(
        &transient_quad, base::Vec2(static_cast<float>(rect.width),
                                    static_cast<float>(rect.height)));

    if (mirror) {
      renderer::Quad::SetTexCoordRect(
          &transient_quad,
          base::Rect(rect.x + rect.width, rect.y, -rect.width, rect.height));
    } else {
      renderer::Quad::SetTexCoordRect(&transient_quad, rect);
    }

    encoder->WriteBuffer(agent->vertex_buffer, 0,
                         reinterpret_cast<uint8_t*>(&transient_quad),
                         sizeof(transient_quad));
  }

  encoder->WriteBuffer(agent->uniform_buffer, 0,
                       reinterpret_cast<const uint8_t*>(&uniform),
                       sizeof(uniform));
}

void GPUOnSpriteRenderingInternal(renderer::RenderDevice* device,
                                  wgpu::RenderPassEncoder* renderpass_encoder,
                                  wgpu::BindGroup* world_binding,
                                  SpriteAgent* agent,
                                  TextureAgent* texture,
                                  int32_t blend_type,
                                  bool wave_active) {
  auto& pipeline_set = device->GetPipelines()->sprite;
  auto* pipeline =
      pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type));

  renderpass_encoder->SetPipeline(*pipeline);
  renderpass_encoder->SetIndexBuffer(**device->GetQuadIndex(),
                                     device->GetQuadIndex()->format());
  renderpass_encoder->SetBindGroup(0, *world_binding);
  renderpass_encoder->SetBindGroup(1, texture->binding);
  renderpass_encoder->SetBindGroup(2, agent->uniform_binding);
  if (wave_active && agent->wave_batch) {
    renderpass_encoder->SetVertexBuffer(0, **agent->wave_batch);
    renderpass_encoder->DrawIndexed(agent->wave_index_count);
  } else {
    renderpass_encoder->SetVertexBuffer(0, agent->vertex_buffer);
    renderpass_encoder->DrawIndexed(6);
  }
}

}  // namespace

scoped_refptr<Sprite> Sprite::New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  ExceptionState& exception_state) {
  return new SpriteImpl(execution_context->graphics,
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
      src_rect_(new RectImpl(base::Rect())),
      color_(new ColorImpl(base::Vec4())),
      tone_(new ToneImpl(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &SpriteImpl::DrawableNodeHandlerInternal, base::Unretained(this)));

  src_rect_observer_ = src_rect_->AddObserver(base::BindRepeating(
      &SpriteImpl::SrcRectChangedInternal, base::Unretained(this)));

  std::memset(&uniform_params_, 0, sizeof(uniform_params_));
  uniform_params_.scale = base::Vec2(1.0f);

  agent_ = new SpriteAgent;
  screen->PostTask(
      base::BindOnce(&GPUCreateSpriteInternal, screen->GetDevice(), agent_));
}

SpriteImpl::~SpriteImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
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
  wave_.phase += wave_.speed / 180.0f;
  wave_.dirty = true;
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
  if (CheckDisposed(exception_state))
    return nullptr;

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

  *src_rect_ = *RectImpl::From(value);
}

scoped_refptr<Viewport> SpriteImpl::Get_Viewport(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return viewport_;
}

void SpriteImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_->GetDrawableController());
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

  return uniform_params_.position.x;
}

void SpriteImpl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.position.x = value;
}

int32_t SpriteImpl::Get_Y(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.position.y;
}

void SpriteImpl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  // Set normal Y attribute for all version.
  uniform_params_.position.y = value;

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

  return uniform_params_.origin.x;
}

void SpriteImpl::Put_Ox(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.origin.x = value;
}

int32_t SpriteImpl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.origin.y;
}

void SpriteImpl::Put_Oy(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.origin.y = value;
}

float SpriteImpl::Get_ZoomX(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.scale.x;
}

void SpriteImpl::Put_ZoomX(const float& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.scale.x = value;
}

float SpriteImpl::Get_ZoomY(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return uniform_params_.scale.y;
}

void SpriteImpl::Put_ZoomY(const float& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  uniform_params_.scale.y = value;
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
  uniform_params_.rotation = angle_ * kPi / 180.0f;
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

  bush_.opacity = value;
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

  opacity_ = value;
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
  if (!bitmap_)
    return;

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    // Batch

    // Singleton
    base::Vec4 composite_color = color_->AsNormColor();
    base::Vec4 flash_color = flash_emitter_.GetColor();
    base::Vec4 target_color = composite_color;
    if (flash_emitter_.IsFlashing())
      target_color =
          (flash_color.w > composite_color.w ? flash_color : composite_color);

    base::Rect src_rect = src_rect_->AsBaseRect();
    uniform_params_.color = target_color;
    uniform_params_.tone = tone_->AsNormColor();
    uniform_params_.bush_depth =
        static_cast<float>(src_rect.y + src_rect.height - bush_.depth);
    uniform_params_.bush_opacity = static_cast<float>(bush_.opacity) / 255.0f;

    screen()->PostTask(
        base::BindOnce(&GPUUpdateSpriteVerticesInternal, params->device,
                       params->command_encoder, agent_, bitmap_->GetAgent(),
                       uniform_params_, src_rect, opacity_, mirror_, wave_,
                       uniform_params_.position, src_rect_dirty_));

    wave_.dirty = false;
    src_rect_dirty_ = false;
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    // Batch

    // Singleton
    screen()->PostTask(
        base::BindOnce(&GPUOnSpriteRenderingInternal, params->device,
                       params->renderpass_encoder, params->world_binding,
                       agent_, bitmap_->GetAgent(), blend_type_, !!wave_.amp));
  }
}

void SpriteImpl::SrcRectChangedInternal() {
  src_rect_dirty_ = true;
}

bool SpriteImpl::IsOtherRenderBatchableInternal(SpriteImpl* other) {
  return other->bitmap_ == bitmap_;
}

}  // namespace content
