// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/media/videodecoder_impl.h"

#include "SDL3/SDL_timer.h"
#include "av1player/src/utils.hpp"

#include "content/canvas/canvas_impl.h"

namespace content {

scoped_refptr<VideoDecoder> VideoDecoder::New(
    ExecutionContext* execution_context,
    const base::String& filename,
    ExceptionState& exception_state) {
  auto* io_service = execution_context->io_service;
  filesystem::IOState io_state;
  SDL_IOStream* stream = io_service->OpenReadRaw(filename, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               io_state.error_message.c_str());
    return nullptr;
  }

  base::OwnedPtr<uvpx::Player> player =
      base::MakeOwnedPtr<uvpx::Player>(uvpx::Player::defaultConfig());
  auto result = player->load(stream, 0, false);

  if (result == uvpx::Player::LoadResult::Success)
    return base::MakeRefCounted<VideoDecoderImpl>(execution_context,
                                                  std::move(player));

  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "Failed to load video decoder.");
  return nullptr;
}

VideoDecoderImpl::VideoDecoderImpl(ExecutionContext* execution_context,
                                   base::OwnedPtr<uvpx::Player> player)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      player_(std::move(player)),
      audio_output_(0),
      audio_stream_(nullptr) {
  // Video info
  base::Vec2i frame_size(player_->info()->width, player_->info()->height);

  // Create agent
  GPUCreateYUVFramesInternal(frame_size);

  // Initialize timer
  last_ticks_ = SDL_GetPerformanceCounter();
  counter_freq_ = SDL_GetPerformanceFrequency();
  frame_delta_ = 1.0f / player_->info()->frameRate;

  // Init audio and frames
  player_->setOnAudioData(OnAudioData, this);
  player_->setOnVideoFinished(OnVideoFinished, this);

  // Extract video info
  auto& info = *player_->info();
  LOG(INFO) << "[VideoDecoder] Resolution: " << info.width << "x"
            << info.height;
  LOG(INFO) << "[VideoDecoder] Frame Rate: " << info.frameRate;

  // Init Audio components
  SDL_AudioSpec wanted_spec;
  wanted_spec.freq = info.audioFrequency;
  wanted_spec.format = SDL_AUDIO_F32;
  wanted_spec.channels = info.audioChannels;

  audio_output_ =
      SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted_spec);
  if (audio_output_) {
    audio_stream_ = SDL_CreateAudioStream(&wanted_spec, &wanted_spec);
    SDL_BindAudioStream(audio_output_, audio_stream_);
  }
}

VideoDecoderImpl::~VideoDecoderImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void VideoDecoderImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool VideoDecoderImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

int32_t VideoDecoderImpl::GetWidth(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return player_->info()->width;
}

int32_t VideoDecoderImpl::GetHeight(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return player_->info()->height;
}

float VideoDecoderImpl::GetDuration(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0.0f;

  return player_->info()->duration;
}

float VideoDecoderImpl::GetFrameRate(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0.0f;

  return player_->info()->frameRate;
}

bool VideoDecoderImpl::HasAudio(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return player_->info()->hasAudio;
}

void VideoDecoderImpl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  // Update frame delta
  player_->update(frame_delta_);

  // Update timer
  uint64_t current_ticks = SDL_GetPerformanceCounter();
  frame_delta_ =
      static_cast<float>(current_ticks - last_ticks_) / counter_freq_;
  last_ticks_ = current_ticks;
}

void VideoDecoderImpl::Render(scoped_refptr<Bitmap> target,
                              ExceptionState& exception_state) {
  auto canvas = CanvasImpl::FromBitmap(target);
  if (!canvas || !canvas->GetAgent())
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid render target.");

  uvpx::Frame* yuv = nullptr;
  if ((yuv = player_->lockRead()) != nullptr) {
    if (yuv->isEmpty()) {
      player_->unlockRead();
      return;
    }

    GPURenderYUVInternal(context()->primary_render_context, yuv,
                         canvas->GetAgent());

    player_->unlockRead();
  }
}

VideoDecoder::PlayState VideoDecoderImpl::Get_State(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return PlayState();

  if (player_->isPlaying())
    return STATE_PLAYING;
  else if (player_->isPaused())
    return STATE_PAUSED;
  else if (player_->isStopped())
    return STATE_STOPPED;
  else if (player_->isFinished())
    return STATE_FINISHED;
  else
    return PlayState();
}

void VideoDecoderImpl::Put_State(const PlayState& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  switch (value) {
    case STATE_PLAYING:
      player_->play();
      if (audio_output_)
        SDL_ResumeAudioDevice(audio_output_);
      break;
    case STATE_PAUSED:
      player_->pause();
      if (audio_output_)
        SDL_PauseAudioDevice(audio_output_);
      break;
    case STATE_STOPPED:
      player_->stop();
      break;
    default:
      break;
  }
}

void VideoDecoderImpl::OnObjectDisposed() {
  if (audio_stream_)
    SDL_DestroyAudioStream(audio_stream_);
  if (audio_output_)
    SDL_CloseAudioDevice(audio_output_);

  audio_stream_ = nullptr;
  player_.reset();

  Agent empty_agent;
  std::swap(agent_, empty_agent);
}

void VideoDecoderImpl::OnAudioData(void* userPtr, float* pcm, size_t count) {
  auto* self = static_cast<VideoDecoderImpl*>(userPtr);
  if (self->audio_stream_)
    SDL_PutAudioStreamData(self->audio_stream_, pcm, count * sizeof(float));
}

void VideoDecoderImpl::OnVideoFinished(void* userPtr) {
  auto* self = static_cast<VideoDecoderImpl*>(userPtr);
  uvpx::debugLog("instace: %p video play finished.", self);
}

void VideoDecoderImpl::GPUCreateYUVFramesInternal(const base::Vec2i& size) {
  auto& render_device = *context()->render_device;

  Diligent::TextureDesc texture_desc;
  texture_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  texture_desc.Format = Diligent::TEX_FORMAT_R8_UNORM;
  texture_desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
  texture_desc.Usage = Diligent::USAGE_DEFAULT;
  texture_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

  texture_desc.Name = "y.plane";
  texture_desc.Width = size.x;
  texture_desc.Height = size.y;
  render_device->CreateTexture(texture_desc, nullptr, &agent_.y);

  texture_desc.Name = "u.plane";
  texture_desc.Width = (size.x + 1) / 2;
  texture_desc.Height = (size.y + 1) / 2;
  render_device->CreateTexture(texture_desc, nullptr, &agent_.u);

  texture_desc.Name = "v.plane";
  render_device->CreateTexture(texture_desc, nullptr, &agent_.v);

  agent_.batch = renderer::QuadBatch::Make(*render_device, 1);
  agent_.shader_binding = render_device.GetPipelines()->yuv.CreateBinding();
}

void VideoDecoderImpl::GPURenderYUVInternal(
    renderer::RenderContext* render_context,
    uvpx::Frame* data,
    BitmapAgent* target) {
  // Update yuv planes
  Diligent::Box dest_box;
  Diligent::TextureSubResData sub_res_data;

  dest_box.MaxX = agent_.y->GetDesc().Width;
  dest_box.MaxY = agent_.y->GetDesc().Height;
  sub_res_data.pData = data->plane(0);
  sub_res_data.Stride = data->width(0);
  (*render_context)
      ->UpdateTexture(agent_.y, 0, 0, dest_box, sub_res_data,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  dest_box.MaxX = agent_.u->GetDesc().Width;
  dest_box.MaxY = agent_.u->GetDesc().Height;
  sub_res_data.pData = data->plane(1);
  sub_res_data.Stride = data->width(1);
  (*render_context)
      ->UpdateTexture(agent_.u, 0, 0, dest_box, sub_res_data,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  dest_box.MaxX = agent_.v->GetDesc().Width;
  dest_box.MaxY = agent_.v->GetDesc().Height;
  sub_res_data.pData = data->plane(2);
  sub_res_data.Stride = data->width(2);
  (*render_context)
      ->UpdateTexture(agent_.v, 0, 0, dest_box, sub_res_data,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Render to target
  auto& render_device = *context()->render_device;
  auto& pipeline_set = render_device.GetPipelines()->yuv;
  auto* pipeline =
      pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND, true);

  // Make transient vertices data
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad,
                                      base::RectF(0.0f, 0.0f, 1.0f, 1.0f));
  agent_.batch.QueueWrite(**render_context, &transient_quad);

  // Setup render target
  float clear_color[] = {0, 0, 0, 0};
  (*render_context)
      ->SetRenderTargets(1, &target->target, target->depth_view,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->ClearDepthStencil(target->depth_view, Diligent::CLEAR_DEPTH_FLAG, 1.0f,
                          0,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->ClearRenderTarget(target->target, clear_color,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Push scissor
  render_context->ScissorState()->Apply(target->size);

  // Setup uniform params
  agent_.shader_binding.u_texture_y->Set(
      agent_.y->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.shader_binding.u_texture_u->Set(
      agent_.u->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.shader_binding.u_texture_v->Set(
      agent_.v->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  (*render_context)->SetPipelineState(pipeline);
  (*render_context)
      ->CommitShaderResources(
          *agent_.shader_binding,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.batch;
  (*render_context)
      ->SetVertexBuffers(0, 1, &vertex_buffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*render_context)
      ->SetIndexBuffer(**render_device.GetQuadIndex(), 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  (*render_context)->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content
