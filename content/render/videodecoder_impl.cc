// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/render/videodecoder_impl.h"

#include "SDL3/SDL_timer.h"

#include "content/canvas/canvas_impl.h"
#include "content/context/execution_context.h"

namespace content {

#if defined(OS_EMSCRIPTEN)

// static
scoped_refptr<VideoDecoder> VideoDecoder::New(
    ExecutionContext* execution_context,
    const std::string& filename,
    int32_t max_frame_delay,
    ExceptionState& exception_state) {
  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "unsupport video playback");
  return nullptr;
}

#else  //! OS_EMSCRIPTEN

// static
scoped_refptr<VideoDecoder> VideoDecoder::New(
    ExecutionContext* execution_context,
    const std::string& filename,
    int32_t max_frame_delay,
    ExceptionState& exception_state) {
  auto* io_service = execution_context->io_service;
  filesystem::IOState io_state;
  SDL_IOStream* stream = io_service->OpenReadRaw(filename, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               io_state.error_message.c_str());
    return nullptr;
  }

  auto playback_configure = uvpx::Player::defaultConfig();
  playback_configure.maxFrameDelay = max_frame_delay;
  playback_configure.decodeThreadsCount = std::thread::hardware_concurrency();

  std::unique_ptr<uvpx::Player> player =
      std::make_unique<uvpx::Player>(std::move(playback_configure));
  auto result = player->load(stream, 0, false);

  if (result == uvpx::Player::LoadResult::Success)
    return base::MakeRefCounted<VideoDecoderImpl>(execution_context,
                                                  std::move(player));

  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "failed to load video decoder");
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// VideoDecoderImpl Implement

VideoDecoderImpl::VideoDecoderImpl(ExecutionContext* execution_context,
                                   std::unique_ptr<uvpx::Player> player)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      player_(std::move(player)),
      audio_stream_(nullptr) {
  // Video info
  base::Vec2i frame_size(player_->info()->width, player_->info()->height);

  // Create frames texture
  GPUCreateYUVFramesInternal(frame_size);

  // Initialize timer
  last_ticks_ = SDL_GetPerformanceCounter();
  counter_freq_ = SDL_GetPerformanceFrequency();
  frame_delta_ = 1.0f / player_->info()->frameRate;

  // Init audio and frames
  player_->setOnAudioData(OnAudioData, this);

  // Extract video info
  auto& info = *player_->info();
  LOG(INFO) << "[VideoDecoder] Resolution: " << info.width << "x"
            << info.height;
  LOG(INFO) << "[VideoDecoder] Frame Rate: " << info.frameRate;

  // Init Audio components
  if (info.hasAudio && context()->audio_server) {
    SDL_AudioSpec desired_spec;
    desired_spec.freq = info.audioFrequency;
    desired_spec.format = SDL_AUDIO_F32;
    desired_spec.channels = info.audioChannels;

    auto device_id = context()->audio_server->GetDeviceID();

    SDL_AudioSpec output_spec;
    SDL_GetAudioDeviceFormat(device_id, &output_spec, nullptr);

    audio_stream_ = SDL_CreateAudioStream(&desired_spec, &output_spec);
    if (audio_stream_)
      SDL_BindAudioStream(device_id, audio_stream_);
  }
}

DISPOSABLE_DEFINITION(VideoDecoderImpl);

int32_t VideoDecoderImpl::GetWidth(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return player_->info()->width;
}

int32_t VideoDecoderImpl::GetHeight(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return player_->info()->height;
}

float VideoDecoderImpl::GetDuration(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return player_->info()->duration;
}

float VideoDecoderImpl::GetFrameRate(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  return player_->info()->frameRate;
}

bool VideoDecoderImpl::HasAudio(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return player_->info()->hasAudio;
}

void VideoDecoderImpl::Update(ExceptionState& exception_state) {
  DISPOSE_CHECK;

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
  if (!Disposable::IsValid(canvas.get()))
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid render target");

  uvpx::Frame* yuv = nullptr;
  if ((yuv = player_->lockRead()) != nullptr) {
    if (yuv->isEmpty())
      return player_->unlockRead();

    // Render YUV plane to bitmap
    GPURenderYUVInternal(context()->primary_render_context,
                         (**canvas)->render_target_view, yuv);

    player_->unlockRead();
  }
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    State,
    VideoDecoder::PlayState,
    VideoDecoderImpl,
    {
      DISPOSE_CHECK_RETURN(STATE_FINISHED);

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
    },
    {
      DISPOSE_CHECK;

      switch (value) {
        case STATE_PLAYING:
          player_->play();
          if (audio_stream_)
            SDL_BindAudioStream(context()->audio_server->GetDeviceID(),
                                audio_stream_);
          break;
        case STATE_PAUSED:
          player_->pause();
          if (audio_stream_)
            SDL_UnbindAudioStream(audio_stream_);
          break;
        case STATE_STOPPED:
          player_->stop();
          break;
        default:
          break;
      }
    });

void VideoDecoderImpl::OnObjectDisposed() {
  player_.reset();

  if (audio_stream_)
    SDL_DestroyAudioStream(audio_stream_);
  audio_stream_ = nullptr;

  GPUData empty_data;
  std::swap(gpu_, empty_data);
}

void VideoDecoderImpl::OnAudioData(void* user_data, float* pcm, size_t count) {
  auto* self = static_cast<VideoDecoderImpl*>(user_data);
  if (self->audio_stream_) {
    SDL_PutAudioStreamData(self->audio_stream_, pcm, count * sizeof(float));
  }
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
  render_device->CreateTexture(texture_desc, nullptr, &gpu_.y);

  texture_desc.Name = "u.plane";
  texture_desc.Width = (size.x + 1) / 2;
  texture_desc.Height = (size.y + 1) / 2;
  render_device->CreateTexture(texture_desc, nullptr, &gpu_.u);

  texture_desc.Name = "v.plane";
  render_device->CreateTexture(texture_desc, nullptr, &gpu_.v);

  gpu_.batch = renderer::QuadBatch::Make(*render_device, 1);
  gpu_.shader_binding = context()->render.pipeline_loader->yuv.CreateBinding();
}

void VideoDecoderImpl::GPURenderYUVInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ITextureView* render_target_view,
    uvpx::Frame* data) {
  // Update yuv planes
  Diligent::Box dest_box;
  Diligent::TextureSubResData sub_res_data;

  dest_box.MaxX = gpu_.y->GetDesc().Width;
  dest_box.MaxY = gpu_.y->GetDesc().Height;
  sub_res_data.pData = data->plane(0);
  sub_res_data.Stride = data->width(0);
  render_context->UpdateTexture(
      gpu_.y, 0, 0, dest_box, sub_res_data,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  dest_box.MaxX = gpu_.u->GetDesc().Width;
  dest_box.MaxY = gpu_.u->GetDesc().Height;
  sub_res_data.pData = data->plane(1);
  sub_res_data.Stride = data->width(1);
  render_context->UpdateTexture(
      gpu_.u, 0, 0, dest_box, sub_res_data,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  dest_box.MaxX = gpu_.v->GetDesc().Width;
  dest_box.MaxY = gpu_.v->GetDesc().Height;
  sub_res_data.pData = data->plane(2);
  sub_res_data.Stride = data->width(2);
  render_context->UpdateTexture(
      gpu_.v, 0, 0, dest_box, sub_res_data,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Render to target
  auto* pipeline = context()->render.pipeline_states->yuv.RawPtr();

  // Make transient vertices data
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad,
                                      base::RectF(0.0f, 0.0f, 1.0f, 1.0f));
  gpu_.batch.QueueWrite(render_context, &transient_quad);

  // Setup render target
  float clear_color[] = {0, 0, 0, 0};
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Setup uniform params
  gpu_.shader_binding.u_texture_y->Set(
      gpu_.y->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  gpu_.shader_binding.u_texture_u->Set(
      gpu_.u->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  gpu_.shader_binding.u_texture_v->Set(
      gpu_.v->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

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
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = context()->render.quad_index->GetIndexType();
  render_context->DrawIndexed(draw_indexed_attribs);
}

#endif  // !OS_EMSCRIPTEN

}  // namespace content
