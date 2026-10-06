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

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "miniaudio.h"

namespace urge {

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

  bool Open(ma_engine* engine,
            ma_sound_group* bus,
            const std::string& filename);
  void Close();

  bool IsOpen() const { return open_; }
  ma_sound* sound() { return &sound_; }

 private:
  std::vector<uint8_t> bytes_;
  ma_decoder decoder_ = {};
  ma_sound sound_ = {};
  bool open_ = false;
};

class AudioStream {
 public:
  ~AudioStream();

  AudioStream(const AudioStream&) = delete;
  AudioStream& operator=(const AudioStream&) = delete;

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

  void SetLooping(bool looping) { looping_ = looping; }

 private:
  friend class AudioService;

  AudioStream(ma_engine* engine, ma_sound_group* bus);

  ma_engine* engine_ = nullptr;
  ma_sound_group* bus_ = nullptr;
  std::string filename_;
  AudioClip clip_;
  ma_uint64 cursor_ = 0;
  bool paused_ = false;
  bool looping_ = false;
};

class AudioEmit {
 public:
  ~AudioEmit();

  AudioEmit(const AudioEmit&) = delete;
  AudioEmit& operator=(const AudioEmit&) = delete;

  bool Play(const std::string& filename, int32_t volume, int32_t pitch);
  void Stop();
  int32_t active() const { return static_cast<int32_t>(queue_.size()); }

 private:
  friend class AudioService;

  AudioEmit(ma_engine* engine, ma_sound_group* bus);

  void Reap(bool all);

  ma_engine* engine_ = nullptr;
  ma_sound_group* bus_ = nullptr;
  std::deque<AudioClip*> queue_;
};

class AudioService {
 public:
  ~AudioService();

  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  static AudioService* Create(const std::string& output = {});

  void SetMasterVolume(int32_t volume);
  int32_t GetMasterVolume() const;

  void SetBusVolume(AudioBus bus, int32_t volume);
  int32_t GetBusVolume(AudioBus bus) const;

  bool SetOutputDevice(const std::string& name);
  const std::string& output_device() const { return output_device_; }
  static std::vector<std::string> OutputDevices();

  AudioStream* CreateStream(AudioBus bus);
  AudioEmit* CreateEmitter();

  static constexpr int32_t kEngineSampleRate = 48000;
  static constexpr int32_t kEngineChannels = 2;

 private:
  struct Kernel;

  AudioService();

  bool OpenOutput(const std::string& name);
  void CloseOutput();

  Kernel* kernel_ = nullptr;
  std::string output_device_;
};

}  // namespace urge
