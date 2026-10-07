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
#include <memory>
#include <string>
#include <vector>

#include "core/definition.h"
#include "core/object.h"

namespace urge {

class AudioEmit;
class AudioService;
class AudioChannel;

URGE_BINDING()
class Audio : public Singleton<Audio> {
 public:
  Audio();
  ~Audio();

  URGE_BINDING()
  void SetupMIDI();

  URGE_BINDING()
  void BGMPlay(std::string filename,
               int32_t volume = 100,
               int32_t pitch = 100,
               float pos = 0.0f);
  URGE_BINDING()
  void BGMStop();
  URGE_BINDING()
  void BGMFade(int32_t time);
  URGE_BINDING()
  float BGMPos();

  URGE_BINDING()
  void BGSPlay(std::string filename,
               int32_t volume = 100,
               int32_t pitch = 100,
               float pos = 0.0f);
  URGE_BINDING()
  void BGSStop();
  URGE_BINDING()
  void BGSFade(int32_t time);
  URGE_BINDING()
  float BGSPos();

  URGE_BINDING()
  void MEPlay(std::string filename, int32_t volume = 100, int32_t pitch = 100);
  URGE_BINDING()
  void MEStop();
  URGE_BINDING()
  void MEFade(int32_t time);

  URGE_BINDING()
  void SEPlay(std::string filename, int32_t volume = 100, int32_t pitch = 100);
  URGE_BINDING()
  void SEStop();

  URGE_BINDING()
  ATTR(int32_t, MasterVolume);
  URGE_BINDING()
  ATTR(int32_t, BGMVolume);
  URGE_BINDING()
  ATTR(int32_t, BGSVolume);
  URGE_BINDING()
  ATTR(int32_t, MEVolume);
  URGE_BINDING()
  ATTR(int32_t, SEVolume);

  std::vector<std::string> OutputDevices();

  const std::string& output_device() const { return output_device_; }

  bool SetOutputDevice(std::string name);

  void Update();

  AudioService* service() { return service_.get(); }

 private:

  void ApplyVolumes();

  std::unique_ptr<AudioService> service_;
  std::unique_ptr<AudioChannel> bgm_;
  std::unique_ptr<AudioChannel> bgs_;
  std::unique_ptr<AudioChannel> me_;
  std::unique_ptr<AudioEmit> se_;

  std::string output_device_;
};

}  // namespace urge
