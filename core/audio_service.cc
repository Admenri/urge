// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/audio_service.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_iostream.h"

#include "core/exception.h"
#include "core/filesystem.h"
#include "core/logger.h"

#ifndef MA_NO_DEVICE_IO
#error \
    "the audio service plays through SDL; build miniaudio with MINIAUDIO_NO_DEVICEIO"
#endif

namespace urge {

namespace {

constexpr size_t kBusCount = 4;

constexpr size_t kMaxVoices = 24;

constexpr size_t BusIndex(AudioBus bus) {
  return static_cast<size_t>(bus);
}

double MillisPerFrame(ma_engine* engine) {
  const ma_uint32 rate = ma_engine_get_sample_rate(engine);
  return rate ? 1000.0 / rate : 0.0;
}

SDL_AudioSpec EngineFormat() {
  SDL_AudioSpec spec = {};
  spec.format = SDL_AUDIO_F32;
  spec.channels = AudioService::kEngineChannels;
  spec.freq = AudioService::kEngineSampleRate;
  return spec;
}

void LogOutput(void*, ma_uint32 level, const char* message) {
  std::string text(message ? message : "");
  if (!text.empty() && text.back() == '\n')
    text.pop_back();
  if (text.empty())
    return;

  switch (level) {
    case MA_LOG_LEVEL_ERROR:
      LOGGER_ERROR("[Audio] {}", text);
      break;
    case MA_LOG_LEVEL_WARNING:
      LOGGER_WARN("[Audio] {}", text);
      break;
    default:
      LOGGER_DEBUG("[Audio] {}", text);
      break;
  }
}

std::vector<uint8_t> ReadWholeFile(const std::string& filename) {
  std::vector<uint8_t> bytes;

  SDL_IOStream* stream = nullptr;
  try {
    IOService::Get().OpenRead(
        filename, [&stream](SDL_IOStream* opened, const std::string&) {
          stream = opened;
          return true;
        });
  } catch (const Exception& error) {
    LOGGER_WARN("audio '{}' could not be opened: {}", filename,
                error.message());
    return bytes;
  }

  if (!stream)
    return bytes;

  const Sint64 size = SDL_GetIOSize(stream);
  if (size > 0) {
    bytes.resize(static_cast<size_t>(size));
    bytes.resize(SDL_ReadIO(stream, bytes.data(), bytes.size()));
  }

  SDL_CloseIO(stream);
  return bytes;
}

struct PullState {
  ma_engine* engine = nullptr;
  size_t frame_size = 0;
  std::vector<uint8_t> scratch;

  static void SDLCALL Callback(void* userdata,
                               SDL_AudioStream* stream,
                               int additional_amount,
                               int total_amount);
};

void SDLCALL PullState::Callback(void* userdata,
                                 SDL_AudioStream* stream,
                                 int additional_amount,
                                 int total_amount) {
  (void)total_amount;

  if (additional_amount <= 0)
    return;

  auto* state = static_cast<PullState*>(userdata);
  const size_t wanted = static_cast<size_t>(additional_amount);
  if (state->scratch.size() < wanted)
    state->scratch.resize(wanted);

  ma_uint64 read = 0;
  ma_engine_read_pcm_frames(state->engine, state->scratch.data(),
                            wanted / state->frame_size, &read);

  const size_t produced = static_cast<size_t>(read) * state->frame_size;
  if (produced < wanted)
    std::memset(state->scratch.data() + produced, 0, wanted - produced);

  SDL_PutAudioStreamData(stream, state->scratch.data(),
                         static_cast<int>(wanted));
}

}  // namespace

AudioClip::~AudioClip() {
  Close();
}

bool AudioClip::Open(ma_engine* engine,
                     ma_sound_group* bus,
                     const std::string& filename) {
  Close();

  if (!engine || filename.empty())
    return false;

  bytes_ = ReadWholeFile(filename);
  if (bytes_.empty())
    return false;

  ma_decoder_config config =
      ma_decoder_config_init(ma_format_f32, ma_engine_get_channels(engine),
                             ma_engine_get_sample_rate(engine));
  if (ma_decoder_init_memory(bytes_.data(), bytes_.size(), &config,
                             &decoder_) != MA_SUCCESS) {
    LOGGER_WARN("audio '{}' is in a format the mixer cannot decode", filename);
    bytes_.clear();
    return false;
  }

  auto* source = reinterpret_cast<ma_data_source*>(&decoder_);
  if (ma_sound_init_from_data_source(engine, source, 0, bus, &sound_) !=
      MA_SUCCESS) {
    LOGGER_WARN("audio '{}' could not be mixed", filename);
    ma_decoder_uninit(&decoder_);
    decoder_ = {};
    bytes_.clear();
    return false;
  }

  open_ = true;
  return true;
}

void AudioClip::Close() {
  if (open_) {
    ma_sound_uninit(&sound_);
    ma_decoder_uninit(&decoder_);
    sound_ = {};
    decoder_ = {};
    open_ = false;
  }

  bytes_.clear();
}

AudioStream::~AudioStream() = default;

AudioStream::AudioStream(ma_engine* engine, ma_sound_group* bus)
    : engine_(engine), bus_(bus) {}

bool AudioStream::Play(const std::string& filename,
                       int32_t volume,
                       int32_t pitch,
                       float pos) {
  if (filename.empty())
    return false;

  if (!clip_.IsOpen() || filename_ != filename) {
    if (!clip_.Open(engine_, bus_, filename)) {
      filename_.clear();
      return false;
    }

    filename_ = filename;
    ma_sound_set_looping(clip_.sound(), looping_ ? MA_TRUE : MA_FALSE);
  }

  paused_ = false;
  cursor_ = 0;

  ma_sound_set_volume(clip_.sound(), volume / 100.0f);
  ma_sound_set_pitch(clip_.sound(), pitch / 100.0f);

  if (pos > 0.0f) {
    const double frames = pos / MillisPerFrame(engine_);
    ma_sound_seek_to_pcm_frame(clip_.sound(), static_cast<ma_uint64>(frames));
  }

  return ma_sound_start(clip_.sound()) == MA_SUCCESS;
}

void AudioStream::Stop() {
  paused_ = false;
  cursor_ = 0;

  if (clip_.IsOpen())
    ma_sound_stop(clip_.sound());
}

void AudioStream::Fade(int32_t time) {
  if (time <= 0) {
    Stop();
    return;
  }

  paused_ = false;
  cursor_ = 0;

  if (clip_.IsOpen())
    ma_sound_stop_with_fade_in_milliseconds(clip_.sound(),
                                            static_cast<ma_uint64>(time));
}

float AudioStream::Pos() {
  if (!clip_.IsOpen())
    return 0.0f;

  ma_uint64 frames = 0;
  if (ma_sound_get_cursor_in_pcm_frames(clip_.sound(), &frames) != MA_SUCCESS)
    return 0.0f;

  return static_cast<float>(frames * MillisPerFrame(engine_));
}

bool AudioStream::IsPlaying() {
  return clip_.IsOpen() && ma_sound_is_playing(clip_.sound()) != MA_FALSE;
}

void AudioStream::Pause() {
  if (!clip_.IsOpen() || paused_)
    return;

  ma_uint64 frames = 0;
  ma_sound_get_cursor_in_pcm_frames(clip_.sound(), &frames);
  cursor_ = frames;
  ma_sound_stop(clip_.sound());
  paused_ = true;
}

void AudioStream::Resume() {
  if (!paused_)
    return;

  paused_ = false;
  if (!clip_.IsOpen())
    return;

  ma_sound_seek_to_pcm_frame(clip_.sound(), cursor_);
  ma_sound_start(clip_.sound());
}

AudioEmit::~AudioEmit() {
  Reap(true);
}

AudioEmit::AudioEmit(ma_engine* engine, ma_sound_group* bus)
    : engine_(engine), bus_(bus) {}

bool AudioEmit::Play(const std::string& filename,
                     int32_t volume,
                     int32_t pitch) {
  if (filename.empty())
    return false;

  Reap(false);

  auto* clip = new AudioClip();
  if (!clip->Open(engine_, bus_, filename)) {
    delete clip;
    return false;
  }

  queue_.push_back(clip);
  ma_sound_set_volume(clip->sound(), volume / 100.0f);
  ma_sound_set_pitch(clip->sound(), pitch / 100.0f);
  ma_sound_start(clip->sound());
  return true;
}

void AudioEmit::Stop() {
  Reap(true);
}

void AudioEmit::Reap(bool all) {
  while (!queue_.empty()) {
    AudioClip* head = queue_.front();
    if (!all && ma_sound_at_end(head->sound()) == MA_FALSE &&
        queue_.size() <= kMaxVoices)
      break;

    delete head;
    queue_.pop_front();
  }
}

struct AudioService::Kernel {
  ma_log log = {};
  bool log_ready = false;
  ma_engine engine = {};
  ma_sound_group bus[kBusCount] = {};
  SDL_AudioSpec engine_format = {};
  SDL_AudioDeviceID device = 0;
  SDL_AudioStream* stream = nullptr;
  PullState pull;
};

AudioService::~AudioService() {
  CloseOutput();

  for (size_t i = 0; i < kBusCount; ++i)
    ma_sound_group_uninit(&kernel_->bus[i]);

  ma_engine_uninit(&kernel_->engine);

  if (kernel_->log_ready)
    ma_log_uninit(&kernel_->log);

  delete kernel_;
}

AudioService::AudioService() : kernel_(new Kernel()) {}

AudioService* AudioService::Create(const std::string& output) {
  std::unique_ptr<AudioService> service(new AudioService());
  Kernel& kernel = *service->kernel_;

  if (ma_log_init(nullptr, &kernel.log) == MA_SUCCESS) {
    kernel.log_ready = true;
    ma_log_register_callback(&kernel.log,
                             ma_log_callback_init(LogOutput, nullptr));
  }

  ma_engine_config engine_config = ma_engine_config_init();
  engine_config.pLog = kernel.log_ready ? &kernel.log : nullptr;
  engine_config.sampleRate = kEngineSampleRate;
  engine_config.channels = kEngineChannels;
  engine_config.noDevice = MA_TRUE;
  if (ma_engine_init(&engine_config, &kernel.engine) != MA_SUCCESS) {
    LOGGER_ERROR("the audio engine did not start");
    return nullptr;
  }

  for (size_t i = 0; i < kBusCount; ++i) {
    if (ma_sound_group_init(&kernel.engine, 0, nullptr, &kernel.bus[i]) !=
        MA_SUCCESS) {
      LOGGER_ERROR("the audio bus {} did not start", i);
      return nullptr;
    }
  }

  kernel.engine_format = EngineFormat();

  kernel.pull.engine = &kernel.engine;
  kernel.pull.frame_size = kEngineChannels * sizeof(float);

  if (!service->OpenOutput(output))
    LOGGER_WARN("no audio output could be opened; the game runs silent");
  else
    LOGGER_INFO("audio output: {}", service->output_device().empty()
                                        ? "system default"
                                        : service->output_device());

  return service.release();
}

void AudioService::SetMasterVolume(int32_t volume) {
  const float gain = std::clamp(volume, 0, 100) / 100.0f;
  ma_engine_set_volume(&kernel_->engine, gain);
}

int32_t AudioService::GetMasterVolume() const {
  return static_cast<int32_t>(ma_engine_get_volume(&kernel_->engine) * 100.0f +
                              0.5f);
}

void AudioService::SetBusVolume(AudioBus bus, int32_t volume) {
  const float gain = std::clamp(volume, 0, 100) / 100.0f;
  ma_sound_group_set_volume(&kernel_->bus[BusIndex(bus)], gain);
}

int32_t AudioService::GetBusVolume(AudioBus bus) const {
  return static_cast<int32_t>(
      ma_sound_group_get_volume(&kernel_->bus[BusIndex(bus)]) * 100.0f + 0.5f);
}

bool AudioService::SetOutputDevice(const std::string& name) {
  if (name == output_device_)
    return true;

  if (!OpenOutput(name))
    return false;

  LOGGER_INFO("audio output: {}",
              output_device_.empty() ? "system default" : output_device_);
  return true;
}

std::vector<std::string> AudioService::OutputDevices() {
  std::vector<std::string> names;

  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
  if (!devices)
    return names;

  names.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const char* label = SDL_GetAudioDeviceName(devices[i]);
    if (label && *label)
      names.emplace_back(label);
  }

  SDL_free(devices);
  return names;
}

AudioStream* AudioService::CreateStream(AudioBus bus) {
  return new AudioStream(&kernel_->engine, &kernel_->bus[BusIndex(bus)]);
}

AudioEmit* AudioService::CreateEmitter() {
  return new AudioEmit(&kernel_->engine,
                       &kernel_->bus[BusIndex(AudioBus::kSE)]);
}

bool AudioService::OpenOutput(const std::string& name) {
  Kernel& kernel = *kernel_;
  CloseOutput();

  SDL_AudioDeviceID target = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  std::string resolved;

  if (!name.empty()) {
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    for (int i = 0; devices && i < count; ++i) {
      const char* label = SDL_GetAudioDeviceName(devices[i]);
      if (label && name == label) {
        target = devices[i];
        resolved = label;
        break;
      }
    }
    if (devices)
      SDL_free(devices);

    if (resolved.empty())
      LOGGER_WARN("audio output '{}' is gone; using the system default", name);
  }

  SDL_AudioSpec device_format = {};
  if (!SDL_GetAudioDeviceFormat(target, &device_format, nullptr)) {
    LOGGER_ERROR("the audio output format could not be read: {}",
                 SDL_GetError());
    return false;
  }
  device_format.format = SDL_AUDIO_F32;

  kernel.device = SDL_OpenAudioDevice(target, &device_format);
  if (!kernel.device) {
    LOGGER_ERROR("the audio output could not be opened: {}", SDL_GetError());
    return false;
  }

  kernel.stream = SDL_CreateAudioStream(&kernel.engine_format, &device_format);
  if (!kernel.stream) {
    LOGGER_ERROR("the audio stream could not be created: {}", SDL_GetError());
    CloseOutput();
    return false;
  }

  if (!SDL_SetAudioStreamGetCallback(kernel.stream, &PullState::Callback,
                                     &kernel.pull) ||
      !SDL_BindAudioStream(kernel.device, kernel.stream)) {
    LOGGER_ERROR("the audio output could not be bound: {}", SDL_GetError());
    CloseOutput();
    return false;
  }

  output_device_ = resolved;
  return true;
}

void AudioService::CloseOutput() {
  Kernel& kernel = *kernel_;

  if (kernel.stream) {
    SDL_DestroyAudioStream(kernel.stream);
    kernel.stream = nullptr;
  }

  if (kernel.device) {
    SDL_CloseAudioDevice(kernel.device);
    kernel.device = 0;
  }
}

}  // namespace urge
