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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/render/gpu_utils.h"

namespace urge::wgsl {

using LayoutGroup = std::pair<uint32_t, std::vector<util::LayoutEntry>>;

enum class BindingKind {
  kUniform,
  kStorage,
  kTexture2D,
  kTextureCube,
  kTextureArray,
  kTextureDepth,
  kSampler,
  kSamplerComparison,
};

struct BindingInfo {
  uint32_t group = 0;
  uint32_t binding = 0;
  std::string name;
  BindingKind kind = BindingKind::kUniform;
  uint64_t size = 0;
};

std::vector<LayoutGroup> Reflect(std::string_view vertex_source,
                                 std::string_view fragment_source);

std::vector<BindingInfo> ReflectBindings(std::string_view vertex_source,
                                         std::string_view fragment_source);

}
