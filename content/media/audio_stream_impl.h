// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MEDIA_AUDIO_STREAM_IMPL_H_
#define CONTENT_MEDIA_AUDIO_STREAM_IMPL_H_

#include "components/audioservice/audio_stream.h"
#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_audiostream.h"

namespace content {

class AudioStreamImpl : public AudioStream,
                        public EngineObject,
                        public Disposable {
 public:
  AudioStreamImpl(ExecutionContext* execution_context,
                  std::unique_ptr<ma_sound> handle);
  ~AudioStreamImpl() override;

  AudioStreamImpl(const AudioStreamImpl&) = delete;
  AudioStreamImpl& operator=(const AudioStreamImpl&) = delete;

 public:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void Start(ExceptionState& exception_state) override;
  void Stop(ExceptionState& exception_state) override;
  void Seek(float time, ExceptionState& exception_state) override;
  float Cursor(ExceptionState& exception_state) override;
  float Length(ExceptionState& exception_state) override;
  bool IsPlaying(ExceptionState& exception_state) override;
  bool AtEnd(ExceptionState& exception_state) override;
  void SetStartTime(uint64_t time, ExceptionState& exception_state) override;
  void SetStopTime(uint64_t time, ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Volume, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Pan, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Pitch, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Loop, bool);

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "AudioStream"; }

  std::unique_ptr<ma_sound> handle_;
};

}  // namespace content

#endif  //! CONTENT_MEDIA_AUDIO_STREAM_IMPL_H_
