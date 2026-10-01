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

#include "core/audio.h"

namespace urge {

Audio::Audio() = default;

Audio::~Audio() = default;

void Audio::SetupMIDI() {}

void Audio::BGMPlay(std::string filename,
                    int32_t volume,
                    int32_t pitch,
                    float pos) {}

void Audio::BGMStop() {}

void Audio::BGMFade(int32_t time) {}

float Audio::BGMPos() {
  return 0.0f;
}

void Audio::BGSPlay(std::string filename,
                    int32_t volume,
                    int32_t pitch,
                    float pos) {}

void Audio::BGSStop() {}

void Audio::BGSFade(int32_t time) {}

float Audio::BGSPos() {
  return 0.0f;
}

void Audio::MEPlay(std::string filename, int32_t volume, int32_t pitch) {}

void Audio::MEStop() {}

void Audio::MEFade(int32_t time) {}

void Audio::SEPlay(std::string filename, int32_t volume, int32_t pitch) {}

void Audio::SEStop() {}

void Audio::Update() {}

}  // namespace urge
