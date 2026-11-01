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
#include <chrono>
#include <cstring>
#include <iterator>
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

constexpr size_t kMaxVoices = 24;

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
    ma_decoding_backend_libvorbis,
    ma_decoding_backend_libopus,
};

constexpr ma_uint32 kDecodingBackendCount =
    static_cast<ma_uint32>(std::size(kDecodingBackends));

constexpr size_t BusIndex(AudioBus bus) {
  return static_cast<size_t>(bus);
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

  if (!service || filename.empty())
    return false;

  // The file reaches memory on the caller's thread; the decoder and the sound
  // that reads it are built on the audio thread.
  bytes_ = ReadWholeFile(filename);
  if (bytes_.empty())
    return false;

  bool opened = false;
  const std::string label = "the audio file '" + filename + "'";
  service->RunSync([&] { opened = Prepare(service, bus, label); });
  return opened;
}

bool AudioClip::OpenMemory(AudioService* service,
                           AudioBus bus,
                           std::vector<uint8_t> bytes) {
  Close();

  if (!service || bytes.empty())
    return false;

  bytes_ = std::move(bytes);

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
  if (ma_decoder_init_memory(bytes_.data(), bytes_.size(), &config,
                             &decoder_) != MA_SUCCESS) {
    LOGGER_WARN("{} is in a format the mixer cannot decode", label);
    bytes_.clear();
    return false;
  }

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

AudioChannel::AudioChannel(AudioService* service, AudioBus bus)
    : service_(service), bus_(bus) {}

AudioChannel::~AudioChannel() {
  clip_.Close();
}

bool AudioChannel::Play(const std::string& filename,
                        int32_t volume,
                        int32_t pitch,
                        float pos) {
  if (!service_ || filename.empty())
    return false;

  if (!clip_.IsOpen() || filename_ != filename) {
    if (!clip_.Open(service_, bus_, filename)) {
      filename_.clear();
      return false;
    }

    filename_ = filename;
  }

  paused_ = false;
  pause_cursor_ = 0;

  bool started = false;
  const float gain = volume / 100.0f;
  const float speed = pitch / 100.0f;
  service_->RunSync([this, gain, speed, pos, &started] {
    ma_sound* sound = clip_.sound();
    ma_sound_set_looping(sound, looping_ ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound, gain);
    ma_sound_set_pitch(sound, speed);

    if (pos > 0.0f) {
      const double frames =
          static_cast<double>(pos) * AudioService::kEngineSampleRate / 1000.0;
      ma_sound_seek_to_pcm_frame(sound, static_cast<ma_uint64>(frames));
    }

    started = ma_sound_start(sound) == MA_SUCCESS;
  });
  return started;
}

void AudioChannel::Stop() {
  paused_ = false;
  pause_cursor_ = 0;

  if (service_ && clip_.IsOpen())
    service_->Post([this] {
      if (clip_.IsOpen())
        ma_sound_stop(clip_.sound());
    });
}

void AudioChannel::Fade(int32_t time) {
  if (time <= 0) {
    Stop();
    return;
  }

  paused_ = false;
  pause_cursor_ = 0;

  if (service_ && clip_.IsOpen())
    service_->Post([this, time] {
      if (clip_.IsOpen()) {
        ma_sound_stop_with_fade_in_milliseconds(clip_.sound(),
                                                static_cast<ma_uint64>(time));
      }
    });
}

float AudioChannel::Pos() {
  if (!clip_.IsOpen())
    return 0.0f;

  return static_cast<float>(static_cast<double>(clip_.cursor_frames()) *
                            1000.0 / AudioService::kEngineSampleRate);
}

bool AudioChannel::IsPlaying() {
  return clip_.IsOpen() && clip_.playing();
}

void AudioChannel::Pause() {
  if (!service_ || !clip_.IsOpen() || paused_)
    return;

  pause_cursor_ = clip_.cursor_frames();
  paused_ = true;

  service_->Post([this] {
    if (clip_.IsOpen())
      ma_sound_stop(clip_.sound());
  });
}

void AudioChannel::Resume() {
  if (!service_ || !paused_ || !clip_.IsOpen())
    return;

  paused_ = false;

  const ma_uint64 frame = pause_cursor_;
  service_->Post([this, frame] {
    if (!clip_.IsOpen())
      return;

    ma_sound* sound = clip_.sound();
    ma_sound_seek_to_pcm_frame(sound, frame);
    ma_sound_start(sound);
  });
}

void AudioChannel::SetLooping(bool looping) {
  looping_ = looping;

  if (service_ && clip_.IsOpen())
    service_->Post([this, looping] {
      if (clip_.IsOpen())
        ma_sound_set_looping(clip_.sound(), looping ? MA_TRUE : MA_FALSE);
    });
}

AudioEmit::AudioEmit(AudioService* service, AudioBus bus)
    : service_(service), bus_(bus) {}

AudioEmit::~AudioEmit() {
  Reap(true);
}

bool AudioEmit::Play(const std::string& filename,
                     int32_t volume,
                     int32_t pitch) {
  if (!service_ || filename.empty())
    return false;

  Reap(false);

  auto clip = std::make_unique<AudioClip>();
  if (!clip->Open(service_, bus_, filename))
    return false;

  AudioClip* sound_clip = clip.get();
  service_->Post([sound_clip, volume, pitch] {
    ma_sound* sound = sound_clip->sound();
    ma_sound_set_volume(sound, volume / 100.0f);
    ma_sound_set_pitch(sound, pitch / 100.0f);
    ma_sound_start(sound);
  });

  queue_.push_back(std::move(clip));
  return true;
}

void AudioEmit::Stop() {
  Reap(true);
}

void AudioEmit::Reap(bool all) {
  std::vector<std::unique_ptr<AudioClip>> dead;
  while (!queue_.empty()) {
    AudioClip* head = queue_.front().get();
    if (!all && !head->at_end() && queue_.size() <= kMaxVoices)
      break;

    dead.push_back(std::move(queue_.front()));
    queue_.pop_front();
  }

  if (dead.empty())
    return;

  // std::function needs a copyable capture, so the owning vector travels
  // behind a shared_ptr; the clips uninit on the audio thread.
  auto doomed = std::make_shared<std::vector<std::unique_ptr<AudioClip>>>(
      std::move(dead));
  service_->Post([doomed] { doomed->clear(); });
}

struct AudioService::Kernel {
  ma_log log = {};
  bool log_ready = false;
  ma_engine engine = {};
  ma_sound_group bus[kBusCount] = {};
  SDL_AudioSpec engine_format = {};
  SDL_AudioDeviceID device = 0;
  SDL_AudioStream* stream = nullptr;

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

AudioChannel* AudioService::CreateStream(AudioBus bus) {
  return new AudioChannel(this, bus);
}

AudioEmit* AudioService::CreateEmitter() {
  return new AudioEmit(this, AudioBus::kSE);
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
    commands_.push_back([&] {
      work();
      {
        std::lock_guard guard(done_mutex);
        done = true;
      }
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
}

}
