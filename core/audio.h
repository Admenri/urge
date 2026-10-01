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

#include <string>

#include "core/object.h"

namespace urge {

// Empty-shell audio subsystem: the public interface is preserved for the
// Ruby bindings and the frame loop, while every operation is a no-op until
// audio is reimplemented on top of SDL3.
class Audio : public Singleton<Audio> {
 public:
  Audio();
  ~Audio();

  /*-export.begin-*/
  void SetupMIDI();

  void BGMPlay(std::string filename,
               int32_t volume = 100,
               int32_t pitch = 100,
               float pos = 0.0f);
  void BGMStop();
  void BGMFade(int32_t time);
  float BGMPos();

  void BGSPlay(std::string filename,
               int32_t volume = 100,
               int32_t pitch = 100,
               float pos = 0.0f);
  void BGSStop();
  void BGSFade(int32_t time);
  float BGSPos();

  void MEPlay(std::string filename, int32_t volume = 100, int32_t pitch = 100);
  void MEStop();
  void MEFade(int32_t time);

  void SEPlay(std::string filename, int32_t volume = 100, int32_t pitch = 100);
  void SEStop();
  /*-export.end-*/

 public:
  // Called once per frame by the application.
  void Update();
};

}  // namespace urge
