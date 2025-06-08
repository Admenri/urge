// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/media/audio_impl.h"

#include "imgui/imgui.h"

#include "content/context/execution_context.h"
#include "content/profile/command_ids.h"

namespace content {

namespace {

void* read_mem_file(SDL_IOStream* src, size_t* datasize, bool freesrc) {
  const int32_t FILE_CHUNK_SIZE = 1024;
  Sint64 size, size_total;
  size_t size_read;
  char *data = NULL, *newdata;
  bool loading_chunks = false;

  if (!src) {
    SDL_InvalidParamError("src");
    return NULL;
  }

  size = SDL_GetIOSize(src);
  if (size < 0) {
    size = FILE_CHUNK_SIZE;
    loading_chunks = true;
  }

  if (static_cast<uint64_t>(size) >= SDL_SIZE_MAX) {
    goto done;
  }

  data = new char[(size_t)(size + 1)];
  if (!data) {
    goto done;
  }

  size_total = 0;
  for (;;) {
    if (loading_chunks) {
      if ((size_total + FILE_CHUNK_SIZE) > size) {
        size = (size_total + FILE_CHUNK_SIZE);
        if (static_cast<uint64_t>(size) >= SDL_SIZE_MAX) {
          newdata = NULL;
        } else {
          delete[] data;
          newdata = new char[(size_t)(size + 1)];
        }
        if (!newdata) {
          delete[] data;
          data = NULL;
          goto done;
        }
        data = newdata;
      }
    }

    size_read = SDL_ReadIO(src, data + size_total, (size_t)(size - size_total));
    if (size_read > 0) {
      size_total += size_read;
      continue;
    }

    /* The stream status will remain set for the caller to check */
    break;
  }

  if (datasize) {
    *datasize = (size_t)size_total;
  }
  data[size_total] = '\0';

done:
  if (freesrc && src) {
    SDL_CloseIO(src);
  }
  return data;
}

void soloud_sdl_audiomixer(void* userdata,
                           SDL_AudioStream* stream,
                           int32_t additional_amount,
                           int32_t total_amount) {
  Uint8* data = static_cast<Uint8*>(SDL_malloc(additional_amount));
  SoLoud::Soloud* soloud = (SoLoud::Soloud*)userdata;
  AudioImpl* manager = static_cast<AudioImpl*>(soloud->mUserData);

  int32_t len = additional_amount;
  short* buf = (short*)data;
  int32_t samples = len / (manager->soloud_spec().channels * sizeof(float));
  soloud->mix((float*)buf, samples);

  SDL_PutAudioStreamData(stream, data, additional_amount);
  SDL_free(data);
}

void soloud_sdl_deinit(SoLoud::Soloud* aSoloud) {
  AudioImpl* manager = static_cast<AudioImpl*>(aSoloud->mUserData);
  SDL_DestroyAudioStream(manager->soloud_stream());
}

SoLoud::result soloud_sdlbackend_init(SoLoud::Soloud* aSoloud,
                                      unsigned int aFlags,
                                      unsigned int aSamplerate,
                                      unsigned int aBuffer,
                                      unsigned int aChannels) {
  AudioImpl* manager = static_cast<AudioImpl*>(aSoloud->mUserData);
  manager->soloud_stream() =
      SDL_CreateAudioStream(&manager->soloud_spec(), &manager->soloud_spec());
  if (!manager->soloud_stream())
    return SoLoud::UNKNOWN_ERROR;
  SDL_SetAudioStreamGetCallback(manager->soloud_stream(), soloud_sdl_audiomixer,
                                aSoloud);

  aSoloud->postinit_internal(manager->soloud_spec().freq, aBuffer, aFlags,
                             manager->soloud_spec().channels);
  aSoloud->mBackendCleanupFunc = soloud_sdl_deinit;

  SDL_BindAudioStream(manager->output_device(), manager->soloud_stream());
  SDL_ResumeAudioDevice(manager->output_device());

  return 0;
}

}  // namespace

AudioImpl::AudioImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context),
      io_service_(execution_context->io_service),
      i18n_profile_(execution_context->i18n_profile),
      output_device_(0) {
  if (execution_context->engine_profile->disable_audio)
    return;

  audio_runner_ = base::ThreadWorker::Create();
  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::InitAudioDeviceInternal,
                                          base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(audio_runner_.get());
}

AudioImpl::~AudioImpl() {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::DestroyAudioDeviceInternal,
                     base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(audio_runner_.get());
}

void AudioImpl::CreateButtonGUISettings() {
  if (ImGui::CollapsingHeader(
          i18n_profile_->GetI18NString(IDS_SETTINGS_AUDIO, "Audio").c_str())) {
    float volume = core_.getGlobalVolume();
    ImGui::SliderFloat(
        i18n_profile_->GetI18NString(IDS_AUDIO_VOLUME, "Volume").c_str(),
        &volume, 0, 1);
    core_.setGlobalVolume(volume);
  }
}

void AudioImpl::SetupMIDI(ExceptionState& exception_state) {
  // TODO: unsupport MIDI
  LOG(WARNING) << "[Content] Unsupport MIDI device setup.";
}

void AudioImpl::BGMPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        int32_t pos,
                        ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::PlaySlotInternal, base::Unretained(this),
                     &bgm_, filename, volume, pitch, pos, true));
}

void AudioImpl::BGMStop(ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(audio_runner_.get(),
                               base::BindOnce(&AudioImpl::StopSlotInternal,
                                              base::Unretained(this), &bgm_));
}

void AudioImpl::BGMFade(int32_t time, ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::FadeSlotInternal,
                                          base::Unretained(this), &bgm_, time));
}

int32_t AudioImpl::BGMPos(ExceptionState& exception_state) {
  if (!output_device_)
    return 0;

  double pos = 0;
  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::GetSlotPosInternal,
                                          base::Unretained(this), &bgm_, &pos));
  base::ThreadWorker::WaitWorkerSynchronize(audio_runner_.get());

  return pos;
}

void AudioImpl::BGSPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        int32_t pos,
                        ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::PlaySlotInternal, base::Unretained(this),
                     &bgs_, filename, volume, pitch, pos, true));
}

void AudioImpl::BGSStop(ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(audio_runner_.get(),
                               base::BindOnce(&AudioImpl::StopSlotInternal,
                                              base::Unretained(this), &bgs_));
}

void AudioImpl::BGSFade(int32_t time, ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::FadeSlotInternal,
                                          base::Unretained(this), &bgs_, time));
}

int32_t AudioImpl::BGSPos(ExceptionState& exception_state) {
  if (!output_device_)
    return 0;

  double pos = 0;
  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::GetSlotPosInternal,
                                          base::Unretained(this), &bgs_, &pos));
  base::ThreadWorker::WaitWorkerSynchronize(audio_runner_.get());

  return pos;
}

void AudioImpl::MEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::PlaySlotInternal, base::Unretained(this), &me_,
                     filename, volume, pitch, 0, false));
}

void AudioImpl::MEStop(ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(audio_runner_.get(),
                               base::BindOnce(&AudioImpl::StopSlotInternal,
                                              base::Unretained(this), &me_));
}

void AudioImpl::MEFade(int32_t time, ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(), base::BindOnce(&AudioImpl::FadeSlotInternal,
                                          base::Unretained(this), &me_, time));
}

void AudioImpl::SEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl ::EmitSoundInternal, base::Unretained(this),
                     filename, volume, pitch));
}

void AudioImpl::SEStop(ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::StopEmitInternal, base::Unretained(this)));
}

void AudioImpl::Reset(ExceptionState& exception_state) {
  if (!output_device_)
    return;

  base::ThreadWorker::PostTask(
      audio_runner_.get(),
      base::BindOnce(&AudioImpl::ResetInternal, base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(audio_runner_.get());
}

void AudioImpl::InitAudioDeviceInternal() {
  LOG(INFO) << "[Content] Running audio thread.";

  soloud_spec_.freq = 44100;
  soloud_spec_.format = SDL_AUDIO_F32;
  soloud_spec_.channels = 2;
  output_device_ =
      SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &soloud_spec_);

  if (!output_device_) {
    LOG(INFO) << "[Content] Failed to initialize audio device, audio module "
                 "will be disabled.";
    return;
  }

  if (core_.init(soloud_sdlbackend_init, this, 0, 0) != SoLoud::SO_NO_ERROR) {
    LOG(INFO) << "[Content] Failed to initialize audio core.";
    return;
  }

  // Me playing monitor thread
  me_watcher_ =
      base::MakeOwnedPtr<std::thread>(&AudioImpl::MeMonitorInternal, this);
}

void AudioImpl::DestroyAudioDeviceInternal() {
  quit_flag_ = true;
  me_watcher_->join();
  me_watcher_.reset();

  SDL_CloseAudioDevice(output_device_);
  LOG(INFO) << "[Content] Finalize audio core.";
}

void AudioImpl::MeMonitorInternal() {
  while (!quit_flag_) {
    if (!core_.getPause(bgm_.play_handle) &&
        core_.isValidVoiceHandle(me_.play_handle)) {
      core_.setPause(bgm_.play_handle, true);
    }

    if (core_.getPause(bgm_.play_handle) &&
        !core_.isValidVoiceHandle(me_.play_handle)) {
      core_.setVolume(bgm_.play_handle, 0);
      core_.fadeVolume(bgm_.play_handle, 1.0f, 0.5);
      core_.setPause(bgm_.play_handle, false);
    }

    std::this_thread::yield();
  }
}

void AudioImpl::PlaySlotInternal(SlotInfo* slot,
                                 const base::String& filename,
                                 int32_t volume,
                                 int32_t pitch,
                                 double pos,
                                 bool loop) {
  if (!core_.isValidVoiceHandle(slot->play_handle) || !slot->source ||
      slot->filename != filename) {
    slot->source = base::MakeOwnedPtr<SoLoud::Wav>();
    slot->filename = filename;

    filesystem::IOState io_state;
    io_service_->OpenRead(
        filename,
        base::BindRepeating(
            [](SoLoud::Wav* loader, SDL_IOStream* ops,
               const base::String& ext) {
              size_t out_size = 0;
              uint8_t* mem =
                  static_cast<uint8_t*>(read_mem_file(ops, &out_size, true));
              return loader->loadMem(mem, out_size) == SoLoud::SO_NO_ERROR;
            },
            slot->source.get()),
        &io_state);
    if (io_state.error_count) {
      LOG(INFO) << "[Content] [Audio] Error: " << io_state.error_message;
      return;
    }

    slot->source->setLooping(loop);
    slot->play_handle = core_.play(*slot->source, volume / 100.0f);
  } else
    core_.setVolume(slot->play_handle, volume / 100.0f);

  core_.setRelativePlaySpeed(slot->play_handle, pitch / 100.0f);
  if (pos)
    core_.seek(slot->play_handle, pos);
}

void AudioImpl::StopSlotInternal(SlotInfo* slot) {
  slot->source.reset();
  slot->filename.clear();
  slot->play_handle = 0;
}

void AudioImpl::FadeSlotInternal(SlotInfo* slot, int32_t time) {
  core_.fadeVolume(slot->play_handle, 0, time * 0.001);
  core_.scheduleStop(slot->play_handle, time * 0.001);
}

void AudioImpl::GetSlotPosInternal(SlotInfo* slot, double* out) {
  *out = core_.getStreamPosition(slot->play_handle);
}

void AudioImpl::EmitSoundInternal(const base::String& filename,
                                  int32_t volume,
                                  int32_t pitch) {
  auto cache = se_cache_.find(filename);
  if (cache != se_cache_.end()) {
    if (se_queue_.size() >= MAX_CHANNELS / 2 - 4) {
      auto invalid_voice = se_queue_.front();
      core_.stop(invalid_voice);
      se_queue_.pop();
    }

    // From cache
    auto handle = core_.play(*cache->second);
    core_.setVolume(handle, volume / 100.0f);
    core_.setRelativePlaySpeed(handle, pitch / 100.0f);
    se_queue_.push(handle);
  } else {
    // Load from filesystem
    base::OwnedPtr<SoLoud::Wav> source = base::MakeOwnedPtr<SoLoud::Wav>();

    filesystem::IOState io_state;
    io_service_->OpenRead(
        filename,
        base::BindRepeating(
            [](SoLoud::Wav* loader, SDL_IOStream* ops,
               const base::String& ext) {
              size_t out_size = 0;
              uint8_t* mem =
                  static_cast<uint8_t*>(read_mem_file(ops, &out_size, true));
              return loader->loadMem(mem, out_size) == SoLoud::SO_NO_ERROR;
            },
            source.get()),
        &io_state);
    if (io_state.error_count) {
      LOG(INFO) << "[Content] [Audio] Error: " << io_state.error_message;
      return;
    }

    if (se_queue_.size() >= MAX_CHANNELS / 2 - 4) {
      auto invalid_voice = se_queue_.front();
      core_.stop(invalid_voice);
      se_queue_.pop();
    }

    // Play new stream
    auto handle = core_.play(*source);
    core_.setVolume(handle, volume / 100.0f);
    core_.setRelativePlaySpeed(handle, pitch / 100.0f);
    se_cache_.emplace(filename, std::move(source));
    se_queue_.push(handle);
  }
}

void AudioImpl::StopEmitInternal() {
  for (auto& it : se_cache_)
    core_.stopAudioSource(*it.second);
}

void AudioImpl::ResetInternal() {
  core_.stopAll();

  bgm_.source.reset();
  bgm_.filename.clear();
  bgm_.play_handle = 0;

  bgs_.source.reset();
  bgs_.filename.clear();
  bgs_.play_handle = 0;

  me_.source.reset();
  me_.filename.clear();
  me_.play_handle = 0;

  se_cache_.clear();
}

}  // namespace content
