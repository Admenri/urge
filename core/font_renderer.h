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
#include <string>
#include <unordered_map>

#include "core/device.h"
#include "core/font.h"

namespace urge {

class FontRenderer {
 public:
  struct TextRun {
    wgpu::Texture texture;
    wgpu::BindGroup group;
    glm::ivec2 size = glm::ivec2(0);
    std::uint8_t opacity = 255;
    std::uint64_t stamp = 0;
  };

  FontRenderer() = default;
  ~FontRenderer() = default;

  FontRenderer(const FontRenderer&) = delete;
  FontRenderer& operator=(const FontRenderer&) = delete;

  TextRun* Acquire(Font& font, const std::string& text);

 private:
  TextRun* Store(std::string key,
                 wgpu::Texture texture,
                 wgpu::BindGroup group,
                 const glm::ivec2& size,
                 std::uint8_t opacity);

  wgpu::Sampler sampler_;
  std::unordered_map<std::string, TextRun> runs_;
  std::size_t bytes_ = 0;
  std::uint64_t stamp_ = 0;
};

}  // namespace urge
