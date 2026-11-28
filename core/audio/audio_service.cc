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

#include "core/audio/audio_service.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_thread.h"

#include "decoders/libopus/miniaudio_libopus.h"
#include "decoders/libvorbis/miniaudio_libvorbis.h"

#include "core/audio/decoders/backend.h"
#include "core/audio/decoders/soundfont.h"
#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/system/filesystem.h"

#ifndef MA_NO_DEVICE_IO
#error \
    "the audio service plays through SDL; build miniaudio with MINIAUDIO_NO_DEVICEIO"
#endif

namespace urge {

namespace {

constexpr size_t kBusCount = 4;
constexpr size_t kMaxSEVoices = 16;
constexpr ma_uint64 kSECoalesceFrames =
    AudioService::kEngineSampleRate * 12 / 1000;
constexpr ma_uint64 kSEAttackFrames = AudioService::kEngineSampleRate / 1000;
constexpr ma_uint64 kSEReleaseFrames = AudioService::kEngineSampleRate * 4 / 1000;

// The audio thread mixes in these units and keeps about this much ahead of the
// device.  A small queue keeps a late wake-up from starving the output without
// making sound effects audibly late.
constexpr ma_uint64 kMixChunkFrames = 512;
constexpr int64_t kTargetLatencyMs = 40;

// The answer is per thread and set at the top of the worker, so it needs no
// lock and cannot race with the worker being started or stopped.
#if !defined(OS_EMSCRIPTEN)
thread_local bool g_on_audio_thread = false;
#endif

ma_decoding_backend_vtable* kDecodingBackends[] = {
    audioformat::DecodingBackend(),
    ma_decoding_backend_libvorbis,
    ma_decoding_backend_libopus,
};

constexpr ma_uint32 kDecodingBackendCount =
    static_cast<ma_uint32>(std::size(kDecodingBackends));

constexpr size_t BusIndex(AudioBus bus) {
  return static_cast<size_t>(bus);
}

bool SoundPlaying(ma_sound* sound) {
  return ma_sound_is_playing(sound) && !ma_sound_at_end(sound);
}

void ConfigurePlayback(ma_sound* sound, int32_t volume, int32_t pitch, float pos) {
  ma_sound_set_volume(sound, volume / 100.0f);
  ma_sound_set_pitch(sound, pitch / 100.0f);
  const double frames = static_cast<double>(pos) *
                        AudioService::kEngineSampleRate / 1000.0;
  if (frames > 0.0 &&
      frames < static_cast<double>(std::numeric_limits<ma_uint64>::max())) {
    ma_sound_seek_to_pcm_frame(sound, static_cast<ma_uint64>(frames));
  }
}

// Apply linked stereo gain before SDL converts the float mix for the device.
// Two blocks (4 ms) of delay let the gain ramp down before an overload, rather
// than clipping individual samples or stepping the gain at a mix boundary.
class OutputLimiter {
 public:
  static void Process(void* userdata, float* samples, ma_uint64 frames) {
    static_cast<OutputLimiter*>(userdata)->Apply(samples, frames);
  }

 private:
  static constexpr size_t kBlockFrames = AudioService::kEngineSampleRate / 500;
  static constexpr float kCeiling = 0.90f;  // Headroom for device resampling.
  static constexpr float kRelease = 0.04f;  // About 50 ms time constant.

  void Apply(float* samples, ma_uint64 frames) {
    for (ma_uint64 i = 0; i < frames; ++i) {
      auto& delayed = delay_[index_];
      for (float& sample : delayed) {
        const float input = *samples;
        block_peak_ = std::max(block_peak_, std::abs(input));
        *samples++ = sample * gain_;
        sample = input;
      }
      gain_ += gain_step_;
      if (++index_ == delay_.size())
        index_ = 0;
      if (index_ % kBlockFrames != 0)
        continue;

      gain_ = target_gain_;
      const float limit = block_peak_ > kCeiling ? kCeiling / block_peak_ : 1.0f;
      // Both ends of the ramp respect the peak of the block being emitted;
      // the far end also respects the next block, which is already buffered.
      target_gain_ = std::min({previous_limit_, limit,
                               gain_ + (1.0f - gain_) * kRelease});
      gain_step_ = (target_gain_ - gain_) / kBlockFrames;
      previous_limit_ = limit;
      block_peak_ = 0.0f;
    }
  }

  std::array<std::array<float, AudioService::kEngineChannels>, kBlockFrames * 2>
      delay_{};
  size_t index_ = 0;
  float block_peak_ = 0.0f;
  float previous_limit_ = 1.0f;
  float gain_ = 1.0f;
  float target_gain_ = 1.0f;
  float gain_step_ = 0.0f;
};

struct SEVoice {
  std::unique_ptr<AudioClip> clip;
  std::string filename;
  int32_t volume;
  int32_t pitch;
  float pos;
  ma_uint64 started_at;
};

struct MusicChannel {
  std::unique_ptr<AudioClip> clip;
  std::string filename;
  bool suspended = false;

  bool Playing() const { return clip && SoundPlaying(clip->sound()); }

  void Suspend() {
    if (!Playing())
      return;

    // Stopping a miniaudio node leaves its decoder cursor unchanged.
    ma_sound_stop(clip->sound());
    suspended = true;
  }

  void Resume() {
    if (suspended) {
      suspended = false;
      ma_sound_start(clip->sound());
    }
  }
};

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
  ExceptionState exception_state;
  IOService::Get().OpenRead(
      exception_state, filename,
      [&stream](SDL_IOStream* opened, const std::string&) {
        stream = opened;
        return true;
      });
  if (exception_state.HadException()) {
    LOGGER_WARN("audio '{}' could not be opened: {}", filename,
                exception_state.message());
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

#if defined(OS_EMSCRIPTEN)

// No threads: the mixer runs inside the pull callback, exactly as the device
// asks for data.
struct PullState {
  ma_engine* engine = nullptr;
  AudioService* service = nullptr;
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

  if (state->service)
    state->service->RefreshClips();
}

#endif  // OS_EMSCRIPTEN

}  // namespace

AudioClip::~AudioClip() {
  Close();
}

bool AudioClip::Open(AudioService* service,
                     AudioBus bus,
                     const std::string& filename) {
  Close();

  if (!service || filename.empty() || !LoadFile(service, filename))
    return false;

  bool opened = false;
  const std::string label = "the audio file '" + filename + "'";
  service->RunSync([&] { opened = Prepare(service, bus, label); });
  return opened;
}

bool AudioClip::LoadFile(AudioService* service, const std::string& filename) {
  // Read on the caller's thread; decode and attach the sound on the mixer.
  bytes_ = ReadWholeFile(filename);
  if (bytes_.empty())
    return false;

  if (audioformat::IsMIDI(bytes_) && !service->SetupMIDI()) {
    bytes_.clear();
    return false;
  }
  return true;
}

bool AudioClip::OpenMemory(AudioService* service,
                           AudioBus bus,
                           std::vector<uint8_t> bytes) {
  Close();

  if (!service || bytes.empty())
    return false;

  bytes_ = std::move(bytes);

  if (audioformat::IsMIDI(bytes_) && !service->SetupMIDI()) {
    bytes_.clear();
    return false;
  }
  bool opened = false;
  service->RunSync(
      [&] { opened = Prepare(service, bus, "the video audio track"); });
  return opened;
}

bool AudioClip::Prepare(AudioService* service,
                        AudioBus bus,
                        const std::string& label) {
  ma_decoder_config config = ma_decoder_config_init(
      ma_format_f32, ma_engine_get_channels(service->engine()),
      ma_engine_get_sample_rate(service->engine()));
  config.ppCustomBackendVTables = kDecodingBackends;
  config.customBackendCount = kDecodingBackendCount;
  config.pCustomBackendUserData = &service->midi_font_;
  if (ma_decoder_init_memory(bytes_.data(), bytes_.size(), &config,
                             &decoder_) != MA_SUCCESS) {
    LOGGER_WARN("{} is in a format the mixer cannot decode", label);
    bytes_.clear();
    return false;
  }

  audioformat::ConfigureLoopPoints(decoder_);
  auto* source = reinterpret_cast<ma_data_source*>(&decoder_);
  if (ma_sound_init_from_data_source(service->engine(), source, 0,
                                     service->bus(bus),
                                     &sound_) != MA_SUCCESS) {
    LOGGER_WARN("{} could not be mixed", label);
    ma_decoder_uninit(&decoder_);
    decoder_ = {};
    bytes_.clear();
    return false;
  }

  service_ = service;
  open_ = true;

  ma_uint64 length = 0;
  if (ma_sound_get_length_in_pcm_frames(&sound_, &length) == MA_SUCCESS)
    length_frames_.store(length, std::memory_order_relaxed);

  service->RegisterClip(this);
  RefreshState();
  return true;
}

void AudioClip::Close() {
  auto release = [this] {
    if (open_) {
      if (service_)
        service_->UnregisterClip(this);

      ma_sound_uninit(&sound_);
      ma_decoder_uninit(&decoder_);
      sound_ = {};
      decoder_ = {};
      open_ = false;
    }

    bytes_.clear();
    service_ = nullptr;
    cursor_frames_.store(0, std::memory_order_relaxed);
    playing_.store(false, std::memory_order_relaxed);
    at_end_.store(false, std::memory_order_relaxed);
  };

  if (service_)
    service_->RunSync(release);
  else
    release();
}

void AudioClip::RefreshState() {
  if (!open_) {
    playing_.store(false, std::memory_order_relaxed);
    at_end_.store(false, std::memory_order_relaxed);
    return;
  }

  ma_uint64 cursor = 0;
  if (ma_sound_get_cursor_in_pcm_frames(&sound_, &cursor) == MA_SUCCESS)
    cursor_frames_.store(cursor, std::memory_order_relaxed);

  playing_.store(ma_sound_is_playing(&sound_) != MA_FALSE,
                 std::memory_order_relaxed);
  at_end_.store(ma_sound_at_end(&sound_) != MA_FALSE,
                std::memory_order_relaxed);
}

void AudioClip::ShutdownFromService() {
  if (open_) {
    ma_sound_uninit(&sound_);
    ma_decoder_uninit(&decoder_);
    sound_ = {};
    decoder_ = {};
    open_ = false;
  }

  bytes_.clear();
  service_ = nullptr;
  cursor_frames_.store(0, std::memory_order_relaxed);
  playing_.store(false, std::memory_order_relaxed);
  at_end_.store(false, std::memory_order_relaxed);
}

struct AudioService::Kernel {
  ma_log log = {};
  bool log_ready = false;
  ma_engine engine = {};
  ma_sound_group bus[kBusCount] = {};
  SDL_AudioSpec engine_format = {};
  SDL_AudioDeviceID device = 0;
  SDL_AudioStream* stream = nullptr;
  OutputLimiter limiter;

  // BGM, BGS and ME are single channels; SE voices are kept in start order.
  std::array<MusicChannel, BusIndex(AudioBus::kSE)> music;
  std::deque<SEVoice> se;
  std::deque<std::unique_ptr<AudioClip>> se_releasing;

  MusicChannel& Music(AudioBus id) { return music[BusIndex(id)]; }

  void PruneSE() {
    std::erase_if(se, [](const auto& voice) {
      return !SoundPlaying(voice.clip->sound());
    });
    std::erase_if(se_releasing, [](const auto& clip) {
      ma_sound* sound = clip->sound();
      return !SoundPlaying(sound) ||
             ma_sound_get_current_fade_volume(sound) == 0.0f;
    });
  }

  void ReleaseSE(SEVoice& voice, ma_uint64 now) {
    // Nothing to fade if the voice has not reached the mixer yet.
    if (voice.started_at == now)
      return;

    // Keep release tails separate from the 16 playable slots, but bounded.
    if (se_releasing.size() == kMaxSEVoices)
      se_releasing.pop_front();

    // Let the fader run to zero before detaching. A miniaudio scheduled stop
    // would skip the whole render block that straddles the stop time.
    ma_sound_set_fade_in_pcm_frames(voice.clip->sound(), -1.0f, 0.0f,
                                   kSEReleaseFrames);
    se_releasing.push_back(std::move(voice.clip));
  }

#if defined(OS_EMSCRIPTEN)
  PullState pull;
#endif
};

AudioService::~AudioService() {
  StopWorker();

  for (AudioClip* clip : clips_)
    clip->ShutdownFromService();
  clips_.clear();

  CloseOutput();

  for (size_t i = 0; i < kBusCount; ++i)
    ma_sound_group_uninit(&kernel_->bus[i]);

  ma_engine_uninit(&kernel_->engine);

  if (kernel_->log_ready)
    ma_log_uninit(&kernel_->log);

  delete kernel_;
}

AudioService::AudioService() : kernel_(new Kernel()) {
  mix_buffer_.assign(kMixChunkFrames * AudioService::kEngineChannels, 0.0f);
}

AudioService* AudioService::Create(const std::string& output,
                                    const std::string& midi_soundfont) {
  std::unique_ptr<AudioService> service(new AudioService());
  service->midi_soundfont_ = midi_soundfont;
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
  engine_config.onProcess = &OutputLimiter::Process;
  engine_config.pProcessUserData = &kernel.limiter;
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

#if defined(OS_EMSCRIPTEN)
  kernel.pull.engine = &kernel.engine;
  kernel.pull.service = service.get();
  kernel.pull.frame_size = kEngineChannels * sizeof(float);
#endif

  if (!service->OpenOutput(output))
    LOGGER_WARN("no audio output could be opened; the game runs silent");
  else
    LOGGER_INFO("audio output: {}", service->output_device().empty()
                                        ? "system default"
                                        : service->output_device());

  service->StartWorker();
  return service.release();
}

bool AudioService::SetupMIDI() {
  if (midi_attempted_)
    return static_cast<bool>(midi_font_);
  midi_attempted_ = true;
  std::vector<std::string> candidates;
  if (!midi_soundfont_.empty())
    candidates.push_back(midi_soundfont_);
#if defined(_WIN32)
  const char* root =
      SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "SystemRoot");
  candidates.push_back(std::string(root ? root : "C:/Windows") +
                       "/System32/drivers/gm.dls");
#elif defined(SDL_PLATFORM_MACOS)
  candidates.push_back(
      "/System/Library/Components/CoreAudio.component/Contents/"
      "Resources/gs_instruments.dls");
#endif
  for (auto path : candidates) {
    std::replace(path.begin(), path.end(), '\\', '/');
    SDL_IOStream* stream = nullptr;
    bool absolute = !path.empty() &&
                    (path[0] == '/' ||
                     (path.size() > 2 && path[1] == ':' && path[2] == '/'));
    if (absolute) {
      stream = SDL_IOFromFile(path.c_str(), "rb");
    } else {
      ExceptionState exception;
      IOService::Get().OpenRead(
          exception, path, [&stream](SDL_IOStream* opened, const std::string&) {
            stream = opened;
            return true;
          });
    }
    if (!stream) {
      LOGGER_WARN("[Audio] MIDI soundfont '{}' could not be opened", path);
      continue;
    }
    Sint64 size = SDL_GetIOSize(stream);
    std::vector<uint8_t> bytes;
    if (size > 0 && size <= 256 * 1024 * 1024) {
      bytes.resize(static_cast<size_t>(size));
      size_t read = SDL_ReadIO(stream, bytes.data(), bytes.size());
      if (read != bytes.size())
        bytes.clear();
    }
    SDL_CloseIO(stream);
    std::string error;
    midi_font_ = audioformat::LoadSoundBank(bytes, error);
    if (midi_font_) {
      LOGGER_INFO("[Audio] MIDI soundfont: {} ({} regions)", path,
                  midi_font_->regions.size());
      return true;
    }
    LOGGER_WARN("[Audio] MIDI soundfont '{}': {}", path, error);
  }
  LOGGER_WARN("[Audio] MIDI disabled: no usable DLS/SF2 soundfont");
  return false;
}

void AudioService::SetMasterVolume(int32_t volume) {
  if (!kernel_)
    return;

  const float gain = std::clamp(volume, 0, 100) / 100.0f;
  Post([this, gain] { ma_engine_set_volume(&kernel_->engine, gain); });
}

void AudioService::SetBusVolume(AudioBus bus, int32_t volume) {
  if (!kernel_)
    return;

  const float gain = std::clamp(volume, 0, 100) / 100.0f;
  Post([this, bus, gain] {
    ma_sound_group_set_volume(&kernel_->bus[BusIndex(bus)], gain);
  });
}

bool AudioService::SetOutputDevice(const std::string& name) {
  if (name == output_device_)
    return true;

  bool opened = false;
  RunSync([this, &name, &opened] { opened = OpenOutput(name); });
  if (!opened)
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

void AudioService::SetPaused(bool paused) {
  if (paused_.exchange(paused) == paused)
    return;

  Post([this, paused] {
    if (!kernel_->device)
      return;

    if (paused)
      SDL_PauseAudioDevice(kernel_->device);
    else
      SDL_ResumeAudioDevice(kernel_->device);
    LOGGER_INFO("audio output {}", paused ? "paused" : "resumed");
  });
}

void AudioService::Play(AudioBus bus,
                        const std::string& filename,
                        int32_t volume,
                        int32_t pitch,
                        float pos) {
  if (filename.empty())
    return;
  if (bus == AudioBus::kSE) {
    PlaySE(filename, volume, pitch, pos);
    return;
  }

  const bool looping = bus == AudioBus::kBGM || bus == AudioBus::kBGS;
  if (looping) {
    bool reused = false;
    RunSync([&] {
      auto& music = kernel_->Music(bus);
      if (music.filename != filename || !(music.suspended || music.Playing()))
        return;

      // Replaying a running loop changes its settings without restarting it.
      ma_sound* sound = music.clip->sound();
      ma_sound_set_stop_time_in_pcm_frames(sound, std::numeric_limits<ma_uint64>::max());
      ma_sound_set_fade_in_pcm_frames(sound, 1.0f, 1.0f, 0);
      ConfigurePlayback(sound, volume, pitch, pos);
      reused = true;
    });
    if (reused)
      return;
  }

  // Load the replacement before touching any playing sound. A missing or
  // invalid file must not pause the BGM.
  auto clip = std::make_unique<AudioClip>();
  if (!clip->Open(this, bus, filename))
    return;

  RunSync([&] {
    ma_sound* sound = clip->sound();
    ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
    ConfigurePlayback(sound, volume, pitch, pos);

    const bool suspended =
        bus == AudioBus::kBGM && kernel_->Music(AudioBus::kME).Playing();
    if (!suspended && ma_sound_start(sound) != MA_SUCCESS)
      return;

    // No mix can run between starting the ME and suspending the BGM, or
    // between replacing one ME with another.
    if (bus == AudioBus::kME)
      kernel_->Music(AudioBus::kBGM).Suspend();
    kernel_->Music(bus) = {std::move(clip), filename, suspended};
  });
}

void AudioService::PlaySE(const std::string& filename,
                          int32_t volume,
                          int32_t pitch,
                          float pos) {
  bool duplicate = false;
  RunSync([&] {
    const ma_uint64 now = ma_engine_get_time_in_pcm_frames(engine());
    const auto& voices = kernel_->se;
    duplicate = std::any_of(
        voices.rbegin(), voices.rend(), [&](const SEVoice& voice) {
          return now - voice.started_at < kSECoalesceFrames &&
                 voice.pitch == pitch && voice.volume == volume &&
                 voice.pos == pos && voice.filename == filename &&
                 SoundPlaying(voice.clip->sound());
        });
  });
  if (duplicate)
    return;

  // Skip the file read and decoder setup entirely for a duplicate trigger.
  auto clip = std::make_unique<AudioClip>();
  if (!clip->LoadFile(this, filename))
    return;

  const std::string label = "the audio file '" + filename + "'";
  RunSync([&] {
    if (!clip->Prepare(this, AudioBus::kSE, label))
      return;

    ma_sound* sound = clip->sound();
    ConfigurePlayback(sound, volume, pitch, pos);
    ma_sound_set_fade_in_pcm_frames(sound, 0.0f, 1.0f, kSEAttackFrames);
    if (ma_sound_start(sound) != MA_SUCCESS)
      return;

    kernel_->PruneSE();
    const ma_uint64 now = ma_engine_get_time_in_pcm_frames(engine());
    auto& voices = kernel_->se;
    if (voices.size() == kMaxSEVoices) {
      kernel_->ReleaseSE(voices.front(), now);
      voices.pop_front();
    }
    voices.push_back({std::move(clip), filename, volume, pitch, pos, now});
  });
}

void AudioService::Stop(AudioBus bus) {
  Post([this, bus] {
    if (bus == AudioBus::kSE) {
      kernel_->PruneSE();
      const ma_uint64 now = ma_engine_get_time_in_pcm_frames(engine());
      for (auto& voice : kernel_->se)
        kernel_->ReleaseSE(voice, now);
      kernel_->se.clear();
      return;
    }

    kernel_->Music(bus) = {};
    if (bus == AudioBus::kME)
      ResumeBGMIfMEEnded();
  });
}

void AudioService::Fade(AudioBus bus, int32_t time) {
  if (time <= 0) {
    Stop(bus);
    return;
  }
  if (bus == AudioBus::kSE)
    return;

  Post([this, bus, time] {
    auto& music = kernel_->Music(bus);
    if (music.suspended) {
      // A silent BGM cannot fade; cancel its scheduled resumption instead.
      music = {};
    } else if (music.Playing()) {
      ma_sound_stop_with_fade_in_milliseconds(music.clip->sound(),
                                              static_cast<ma_uint64>(time));
    }
  });
}

float AudioService::Pos(AudioBus bus) {
  if (bus == AudioBus::kSE)
    return 0.0f;

  ma_uint64 frames = 0;
  RunSync([&] {
    const auto& music = kernel_->Music(bus);
    if (music.clip)
      ma_sound_get_cursor_in_pcm_frames(music.clip->sound(), &frames);
  });
  return static_cast<float>(static_cast<double>(frames) * 1000.0 /
                            kEngineSampleRate);
}

void AudioService::ResumeBGMIfMEEnded() {
  auto& bgm = kernel_->Music(AudioBus::kBGM);
  if (bgm.suspended && !kernel_->Music(AudioBus::kME).Playing())
    bgm.Resume();
}

void AudioService::Post(std::function<void()> work) {
#if defined(OS_EMSCRIPTEN)
  if (work)
    work();
#else
  if (!work)
    return;

  if (!running_.load(std::memory_order_relaxed) || OnAudioThread()) {
    work();
    return;
  }

  {
    std::lock_guard lock(command_mutex_);
    commands_.push_back(std::move(work));
  }
  command_cv_.notify_one();
#endif
}

void AudioService::RunSync(const std::function<void()>& work) {
#if defined(OS_EMSCRIPTEN)
  work();
#else
  if (!running_.load(std::memory_order_relaxed) || OnAudioThread()) {
    work();
    return;
  }

  std::mutex done_mutex;
  std::condition_variable done_cv;
  bool done = false;

  {
    std::lock_guard lock(command_mutex_);
    commands_.push_back([&, work] {
      work();
      // Keep the caller from observing completion and destroying its stack
      // mutex/CV until notification has finished. The predicate may already
      // be true before the caller enters wait(), without any notification.
      std::lock_guard guard(done_mutex);
      done = true;
      done_cv.notify_one();
    });
  }
  command_cv_.notify_one();

  std::unique_lock lock(done_mutex);
  done_cv.wait(lock, [&] { return done; });
#endif
}

bool AudioService::OnAudioThread() const {
#if defined(OS_EMSCRIPTEN)
  return true;
#else
  return g_on_audio_thread;
#endif
}

void AudioService::RegisterClip(AudioClip* clip) {
  clips_.push_back(clip);
}

void AudioService::UnregisterClip(AudioClip* clip) {
  clips_.erase(std::remove(clips_.begin(), clips_.end(), clip), clips_.end());
}

void AudioService::RefreshClips() {
  // The web pull callback also reaches here after mixing. The RGSS handoff
  // must happen even when the game is not calling Graphics.update.
  ResumeBGMIfMEEnded();
  kernel_->PruneSE();
  for (AudioClip* clip : clips_)
    clip->RefreshState();
}

ma_engine* AudioService::engine() {
  return &kernel_->engine;
}

ma_sound_group* AudioService::bus(AudioBus bus) {
  return &kernel_->bus[BusIndex(bus)];
}

void AudioService::StartWorker() {
#if !defined(OS_EMSCRIPTEN)
  running_.store(true, std::memory_order_relaxed);
  thread_ = std::thread(&AudioService::WorkerLoop, this);
#endif
}

void AudioService::StopWorker() {
#if !defined(OS_EMSCRIPTEN)
  if (!running_.exchange(false))
    return;

  command_cv_.notify_all();
  if (thread_.joinable())
    thread_.join();

  // Commands left behind may own clips; release them here, on the caller, now
  // that the engine has no reader.
  std::deque<std::function<void()>> pending;
  {
    std::lock_guard lock(command_mutex_);
    pending.swap(commands_);
  }
#endif
}

void AudioService::WorkerLoop() {
#if !defined(OS_EMSCRIPTEN)
  g_on_audio_thread = true;
  SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);

  while (running_.load(std::memory_order_relaxed)) {
    {
      std::unique_lock lock(command_mutex_);
      command_cv_.wait_for(
          lock, std::chrono::milliseconds(paused_.load() ? 50 : 5), [this] {
            return !commands_.empty() ||
                   !running_.load(std::memory_order_relaxed);
          });

      std::deque<std::function<void()>> commands;
      commands.swap(commands_);
      lock.unlock();

      for (auto& command : commands)
        if (command)
          command();
    }

    if (!running_.load(std::memory_order_relaxed))
      break;

    if (!paused_.load(std::memory_order_relaxed))
      MixToStream();

    RefreshClips();
  }

  g_on_audio_thread = false;
#endif
}

void AudioService::MixToStream() {
  Kernel& kernel = *kernel_;
  if (!kernel.stream)
    return;

  const int64_t target = static_cast<int64_t>(kEngineSampleRate) *
                         kEngineChannels * static_cast<int64_t>(sizeof(float)) *
                         kTargetLatencyMs / 1000;

  int64_t queued = SDL_GetAudioStreamQueued(kernel.stream);
  if (queued < 0)
    queued = 0;

  while (queued < target) {
    ma_uint64 read = 0;
    ma_engine_read_pcm_frames(&kernel.engine, mix_buffer_.data(),
                              kMixChunkFrames, &read);

    if (read < kMixChunkFrames) {
      std::fill(mix_buffer_.begin() + read * kEngineChannels, mix_buffer_.end(),
                0.0f);
    }

    const int bytes = static_cast<int>(mix_buffer_.size() * sizeof(float));
    if (!SDL_PutAudioStreamData(kernel.stream, mix_buffer_.data(), bytes))
      break;

    queued += bytes;
    if (read == 0)
      break;
  }
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
        resolved = name;
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

#if defined(OS_EMSCRIPTEN)
  if (!SDL_SetAudioStreamGetCallback(kernel.stream, &PullState::Callback,
                                     &kernel.pull)) {
    LOGGER_ERROR("the audio output could not be fed: {}", SDL_GetError());
    CloseOutput();
    return false;
  }
#endif

  if (!SDL_BindAudioStream(kernel.device, kernel.stream)) {
    LOGGER_ERROR("the audio output could not be bound: {}", SDL_GetError());
    CloseOutput();
    return false;
  }

  if (paused_.load(std::memory_order_relaxed))
    SDL_PauseAudioDevice(kernel.device);

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
  kernel.limiter = {};
}

}
