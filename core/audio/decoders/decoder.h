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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace urge::audioformat {
// Decoders emit interleaved float PCM; the mixer owns resampling and gain.
class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual size_t Read(float* output, size_t frames) = 0;
  virtual bool Seek(uint64_t frame) = 0;
  virtual uint64_t Cursor() const = 0;
  virtual uint64_t Length() const = 0;
  virtual uint64_t DamagedFrames() const { return 0; }
  virtual uint64_t LoopStart() const { return 0; }
  virtual uint64_t LoopEnd() const { return Length(); }
  virtual uint32_t Rate() const = 0;
  virtual uint32_t Channels() const = 0;
};
struct SoundBank;
std::shared_ptr<const SoundBank> LoadSoundBank(std::span<const uint8_t> bytes,
                                               std::string& error);
std::unique_ptr<Decoder> OpenMIDI(std::span<const uint8_t> bytes,
                                  std::shared_ptr<const SoundBank> bank,
                                  std::string& error);
std::unique_ptr<Decoder> OpenWMA(std::span<const uint8_t> bytes,
                                 std::string& error);
bool IsMIDI(std::span<const uint8_t> bytes);
bool IsWMA(std::span<const uint8_t> bytes);
}  // namespace urge::audioformat
