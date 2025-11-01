// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/media/audio_stream_impl.h"

#include "magic_enum/magic_enum.hpp"

#include "content/context/execution_context.h"

namespace content {

scoped_refptr<AudioStream> AudioStream::New(ExecutionContext* execution_context,
                                            const std::string& filename,
                                            ExceptionState& exception_state) {
  auto* device_engine = execution_context->audio_server->GetRawEngine();

  auto sound_handle = std::make_unique<ma_sound>();
  auto result = ma_sound_init_from_file(device_engine, filename.c_str(),
                                        MA_SOUND_FLAG_ASYNC, nullptr, nullptr,
                                        sound_handle.get());

  if (result != MA_SUCCESS) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               magic_enum::enum_name(result).data());
    return nullptr;
  }

  return base::MakeRefCounted<AudioStreamImpl>(execution_context,
                                               std::move(sound_handle));
}

AudioStreamImpl::AudioStreamImpl(ExecutionContext* execution_context,
                                 std::unique_ptr<ma_sound> handle)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      handle_(std::move(handle)) {}

DISPOSABLE_DEFINITION(AudioStreamImpl);

void AudioStreamImpl::Start(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  ma_sound_start(handle_.get());
}

void AudioStreamImpl::Stop(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  ma_sound_stop(handle_.get());
}

void AudioStreamImpl::Seek(float time, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  ma_sound_seek_to_second(handle_.get(), time);
}

float AudioStreamImpl::Cursor(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  float result;
  ma_sound_get_cursor_in_seconds(handle_.get(), &result);

  return result;
}

float AudioStreamImpl::Length(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0.0f);

  float result;
  ma_sound_get_length_in_seconds(handle_.get(), &result);

  return result;
}

bool AudioStreamImpl::IsPlaying(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return ma_sound_is_playing(handle_.get());
}

bool AudioStreamImpl::AtEnd(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return ma_sound_at_end(handle_.get());
}

void AudioStreamImpl::SetStartTime(uint64_t time,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  ma_sound_set_start_time_in_pcm_frames(handle_.get(), time);
}

void AudioStreamImpl::SetStopTime(uint64_t time,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK;

  ma_sound_set_stop_time_in_pcm_frames(handle_.get(), time);
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Volume,
    float,
    AudioStreamImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);

      return ma_sound_get_volume(handle_.get());
    },
    {
      DISPOSE_CHECK;

      ma_sound_set_volume(handle_.get(), value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Pan,
    float,
    AudioStreamImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);

      return ma_sound_get_pan(handle_.get());
    },
    {
      DISPOSE_CHECK;

      ma_sound_set_pan(handle_.get(), value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Pitch,
    float,
    AudioStreamImpl,
    {
      DISPOSE_CHECK_RETURN(0.0f);

      return ma_sound_get_pitch(handle_.get());
    },
    {
      DISPOSE_CHECK;

      ma_sound_set_pitch(handle_.get(), value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Loop,
    bool,
    AudioStreamImpl,
    {
      DISPOSE_CHECK_RETURN(false);

      return ma_sound_is_looping(handle_.get());
    },
    {
      DISPOSE_CHECK;

      ma_sound_set_looping(handle_.get(), value);
    });

void AudioStreamImpl::OnObjectDisposed() {
  ma_sound_uninit(handle_.get());
  handle_.reset();
}

}  // namespace content
