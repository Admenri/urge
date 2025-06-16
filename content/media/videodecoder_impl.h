// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MEDIA_VIDEODECODER_H_
#define CONTENT_MEDIA_VIDEODECODER_H_

#include "SDL3/SDL_audio.h"
#include "av1player/src/player.hpp"

#include "base/memory/ref_counted.h"
#include "components/filesystem/io_service.h"
#include "content/context/disposable.h"
#include "content/public/engine_videodecoder.h"
#include "content/screen/renderscreen_impl.h"

namespace content {

class VideoDecoderImpl : public VideoDecoder,
                         public EngineObject,
                         public Disposable {
 public:
  struct Agent {
    RRefPtr<Diligent::ITexture> y, u, v;
    renderer::QuadBatch batch;
    renderer::Binding_YUV shader_binding;
  };

  VideoDecoderImpl(ExecutionContext* execution_context,
                   base::OwnedPtr<uvpx::Player> player);
  ~VideoDecoderImpl() override;

  VideoDecoderImpl(const VideoDecoderImpl&) = delete;
  VideoDecoderImpl& operator=(const VideoDecoderImpl&) = delete;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  int32_t GetWidth(ExceptionState& exception_state) override;
  int32_t GetHeight(ExceptionState& exception_state) override;
  float GetDuration(ExceptionState& exception_state) override;
  float GetFrameRate(ExceptionState& exception_state) override;
  bool HasAudio(ExceptionState& exception_state) override;
  void Update(ExceptionState& exception_state) override;
  void Render(scoped_refptr<Bitmap> target,
              ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(State, PlayState);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "DAV1D/AV1 Decoder"; }

  static void OnAudioData(void* userPtr, float* pcm, size_t count);
  static void OnVideoFinished(void* userPtr);

  void GPUCreateYUVFramesInternal(const base::Vec2i& size);
  void GPURenderYUVInternal(renderer::RenderContext* render_context,
                            uvpx::Frame* data,
                            BitmapAgent* target);

  Agent agent_;

  base::OwnedPtr<uvpx::Player> player_;
  uint64_t last_ticks_;
  int64_t counter_freq_;
  float frame_delta_;

  SDL_AudioDeviceID audio_output_;
  SDL_AudioStream* audio_stream_;
};

}  // namespace content

#endif  //! CONTENT_MEDIA_VIDEODECODER_H_
