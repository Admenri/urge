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

class AudioClip;

URGE_BINDING()
class AudioStream : public Object {
 public:
  URGE_BINDING()
  AudioStream(std::string filename);
  AudioStream(std::vector<uint8_t> data, float length);
  URGE_BINDING()
  ~AudioStream() override;

  URGE_BINDING()
  void Start();
  URGE_BINDING()
  void Stop();
  URGE_BINDING()
  void Seek(float pos);
  URGE_BINDING()
  float Cursor();
  URGE_BINDING()
  float Length();
  URGE_BINDING(Name : "playing?")
  bool IsPlaying();
  URGE_BINDING(Name : "end?")
  bool IsEnd();
  URGE_BINDING()
  void SetStartTime(float pos);
  URGE_BINDING()
  void SetStopTime(float pos);

  URGE_BINDING()
  ATTR(float, Volume);
  URGE_BINDING()
  ATTR(float, Pan);
  URGE_BINDING()
  ATTR(float, Pitch);
  URGE_BINDING()
  ATTR(bool, Loop);

 private:
  void ApplyState();
  void ApplyLoopPoint();

  std::unique_ptr<AudioClip> clip_;
  float start_time_ = 0.0f;
  float stop_time_ = 0.0f;
  float volume_ = 1.0f;
  float pan_ = 0.0f;
  float pitch_ = 1.0f;
  float length_ = -1.0f;
  bool loop_ = false;
};

}  // namespace urge
