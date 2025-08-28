// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_VIDEODECODER_H_
#define CONTENT_RENDER_VIDEODECODER_H_

#include "SDL3/SDL_audio.h"

#include "base/memory/ref_counted.h"
#include "components/filesystem/io_service.h"
#include "content/context/disposable.h"
#include "content/public/engine_videodecoder.h"
#include "content/screen/renderscreen_impl.h"

#if !defined(OS_EMSCRIPTEN)
#include "av1player/src/player.hpp"

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
                   std::unique_ptr<uvpx::Player> player);
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
  std::string DisposedObjectName() override { return "DAV1D/AV1 Decoder"; }

  static void OnAudioData(void* user_data, float* pcm, size_t count);

  void GPUCreateYUVFramesInternal(const base::Vec2i& size);
  void GPURenderYUVInternal(Diligent::IDeviceContext* render_context,
                            uvpx::Frame* data,
                            BitmapAgent* target);

  Agent agent_;

  std::unique_ptr<uvpx::Player> player_;
  SDL_AudioStream* audio_stream_;
  uint64_t last_ticks_;
  int64_t counter_freq_;
  float frame_delta_;
};

}  // namespace content
#endif  //! defined(OS_EMSCRIPTEN)

#endif  //! CONTENT_MEDIA_VIDEODECODER_H_
