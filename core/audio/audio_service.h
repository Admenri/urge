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

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if !defined(OS_EMSCRIPTEN)
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

#include "miniaudio.h"

namespace urge {

class AudioService;

enum class AudioBus {
  kBGM,
  kBGS,
  kME,
  kSE,
};

class AudioClip {
 public:
  AudioClip() = default;
  ~AudioClip();

  AudioClip(const AudioClip&) = delete;
  AudioClip& operator=(const AudioClip&) = delete;

  bool Open(AudioService* service, AudioBus bus, const std::string& filename);
  bool OpenMemory(AudioService* service,
                  AudioBus bus,
                  std::vector<uint8_t> bytes);
  void Close();

  bool IsOpen() const { return open_; }

  // Only valid on the audio thread; the sound and its decoder belong to it.
  ma_sound* sound() { return &sound_; }

  // Snapshots refreshed by the audio thread, safe to read from anywhere.
  ma_uint64 cursor_frames() const {
    return cursor_frames_.load(std::memory_order_relaxed);
  }
  ma_uint64 length_frames() const {
    return length_frames_.load(std::memory_order_relaxed);
  }
  bool playing() const { return playing_.load(std::memory_order_relaxed); }
  bool at_end() const { return at_end_.load(std::memory_order_relaxed); }

 private:
  friend class AudioService;

  bool Prepare(AudioService* service, AudioBus bus, const std::string& label);
  void RefreshState();
  // Runs on the audio thread when the service is torn down; the owner still
  // holds the clip and will see it closed.
  void ShutdownFromService();

  AudioService* service_ = nullptr;
  std::vector<uint8_t> bytes_;
  ma_decoder decoder_ = {};
  ma_sound sound_ = {};
  bool open_ = false;

  std::atomic<ma_uint64> cursor_frames_{0};
  std::atomic<ma_uint64> length_frames_{0};
  std::atomic<bool> playing_{false};
  std::atomic<bool> at_end_{false};
};

class AudioChannel {
 public:
  AudioChannel(AudioService* service, AudioBus bus);
  ~AudioChannel();

  AudioChannel(const AudioChannel&) = delete;
  AudioChannel& operator=(const AudioChannel&) = delete;

  bool Play(const std::string& filename,
            int32_t volume,
            int32_t pitch,
            float pos = 0.0f);
  void Stop();
  void Fade(int32_t time);
  float Pos();

  bool IsPlaying();
  bool IsPausing() const { return paused_; }
  void Pause();
  void Resume();

  void SetLooping(bool looping);

 private:
  AudioService* service_ = nullptr;
  AudioBus bus_ = AudioBus::kBGM;
  std::string filename_;
  AudioClip clip_;
  ma_uint64 pause_cursor_ = 0;
  bool paused_ = false;
  bool looping_ = false;
};

class AudioEmit {
 public:
  AudioEmit(AudioService* service, AudioBus bus);
  ~AudioEmit();

  AudioEmit(const AudioEmit&) = delete;
  AudioEmit& operator=(const AudioEmit&) = delete;

  bool Play(const std::string& filename, int32_t volume, int32_t pitch);
  void Stop();
  int32_t active() const { return static_cast<int32_t>(queue_.size()); }

 private:
  void Reap(bool all);

  AudioService* service_ = nullptr;
  AudioBus bus_ = AudioBus::kSE;
  std::deque<std::unique_ptr<AudioClip>> queue_;
};

class AudioService {
 public:
  ~AudioService();

  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  static AudioService* Create(const std::string& output = {});

  void SetMasterVolume(int32_t volume);

  void SetBusVolume(AudioBus bus, int32_t volume);

  bool SetOutputDevice(const std::string& name);
  const std::string& output_device() const { return output_device_; }
  static std::vector<std::string> OutputDevices();

  // Stops the SDL device without tearing it down: no callback runs, so the
  // miniaudio engine and every stream on top of it stay frozen in place.
  void SetPaused(bool paused);
  bool paused() const { return paused_.load(std::memory_order_relaxed); }

  AudioChannel* CreateStream(AudioBus bus);
  AudioEmit* CreateEmitter();

  // The engine belongs to the audio thread on everything that can run one.
  // Post() runs WORK there without waiting; RunSync() waits and runs inline
  // when it is already on that thread (or when the platform is single
  // threaded).  Every decoder and mixer call goes through these two, which is
  // what keeps that work off the frame loop.
  void Post(std::function<void()> work);
  void RunSync(const std::function<void()>& work);
  bool OnAudioThread() const;

  void RegisterClip(AudioClip* clip);
  void UnregisterClip(AudioClip* clip);
  void RefreshClips();

  static constexpr int32_t kEngineSampleRate = 48000;
  static constexpr int32_t kEngineChannels = 2;

 private:
  friend class AudioClip;

  struct Kernel;

  AudioService();

  bool OpenOutput(const std::string& name);
  void CloseOutput();
  void StartWorker();
  void StopWorker();
  void WorkerLoop();
  void MixToStream();

  // Audio thread only.
  ma_engine* engine();
  ma_sound_group* bus(AudioBus bus);

  Kernel* kernel_ = nullptr;
  std::string output_device_;
  std::atomic<bool> paused_{false};

  // Audio thread only.  The destructor touches it after the thread is joined.
  std::vector<AudioClip*> clips_;
  std::vector<float> mix_buffer_;

#if !defined(OS_EMSCRIPTEN)
  std::thread thread_;
  std::mutex command_mutex_;
  std::condition_variable command_cv_;
  std::deque<std::function<void()>> commands_;
  std::atomic<bool> running_{false};
#endif
};

}
