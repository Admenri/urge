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

#include "core/render/texture_format.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace urge {

namespace {

std::string Normalize(std::string_view name) {
  std::string result;
  result.reserve(name.size());
  for (const char ch : name) {
    if (ch == '_' || ch == '-' || ch == ' ')
      continue;
    result.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return result;
}

}

wgpu::TextureFormat ParseTextureFormat(std::string_view name) {
  static const std::unordered_map<std::string, wgpu::TextureFormat> kFormats = {
      {"rgba8unorm", wgpu::TextureFormat::RGBA8Unorm},
      {"rgba8unormsrgb", wgpu::TextureFormat::RGBA8UnormSrgb},
      {"bgra8unorm", wgpu::TextureFormat::BGRA8Unorm},
      {"bgra8unormsrgb", wgpu::TextureFormat::BGRA8UnormSrgb},
      {"rgba16float", wgpu::TextureFormat::RGBA16Float},
      {"rgba32float", wgpu::TextureFormat::RGBA32Float},
      {"rg11b10ufloat", wgpu::TextureFormat::RG11B10Ufloat},
      {"rgb10a2unorm", wgpu::TextureFormat::RGB10A2Unorm},
      {"rg16float", wgpu::TextureFormat::RG16Float},
      {"r32float", wgpu::TextureFormat::R32Float},
      {"rgba8snorm", wgpu::TextureFormat::RGBA8Snorm},
      {"r8unorm", wgpu::TextureFormat::R8Unorm},
      {"depth32float", wgpu::TextureFormat::Depth32Float},
      {"depth24plus", wgpu::TextureFormat::Depth24Plus},
      {"depth24plusstencil8", wgpu::TextureFormat::Depth24PlusStencil8},
      {"depth16unorm", wgpu::TextureFormat::Depth16Unorm},
  };

  const std::string key = Normalize(name);
  const auto it = kFormats.find(key);
  if (it != kFormats.end())
    return it->second;

  if (key == "screen" || key == "swapchain" || key == "surface")
    return wgpu::TextureFormat::BGRA8Unorm;

  return wgpu::TextureFormat::Undefined;
}

std::string TextureFormatName(wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::RGBA8Unorm:
      return "rgba8unorm";
    case wgpu::TextureFormat::RGBA8UnormSrgb:
      return "rgba8unorm-srgb";
    case wgpu::TextureFormat::BGRA8Unorm:
      return "bgra8unorm";
    case wgpu::TextureFormat::BGRA8UnormSrgb:
      return "bgra8unorm-srgb";
    case wgpu::TextureFormat::RGBA16Float:
      return "rgba16float";
    case wgpu::TextureFormat::RGBA32Float:
      return "rgba32float";
    case wgpu::TextureFormat::RG11B10Ufloat:
      return "rg11b10ufloat";
    case wgpu::TextureFormat::RGB10A2Unorm:
      return "rgb10a2unorm";
    case wgpu::TextureFormat::RG16Float:
      return "rg16float";
    case wgpu::TextureFormat::R32Float:
      return "r32float";
    case wgpu::TextureFormat::Depth32Float:
      return "depth32float";
    case wgpu::TextureFormat::Depth24Plus:
      return "depth24plus";
    case wgpu::TextureFormat::Depth24PlusStencil8:
      return "depth24plus-stencil8";
    default:
      return "undefined";
  }
}

bool IsDepthFormat(wgpu::TextureFormat format) {
  return format == wgpu::TextureFormat::Depth32Float ||
         format == wgpu::TextureFormat::Depth24Plus ||
         format == wgpu::TextureFormat::Depth24PlusStencil8 ||
         format == wgpu::TextureFormat::Depth16Unorm;
}

bool IsStencilFormat(wgpu::TextureFormat format) {
  return format == wgpu::TextureFormat::Depth24PlusStencil8 ||
         format == wgpu::TextureFormat::Stencil8;
}

bool IsStorageCapable(wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8Snorm:
    case wgpu::TextureFormat::RGBA8Uint:
    case wgpu::TextureFormat::RGBA8Sint:
    case wgpu::TextureFormat::R8Unorm:
    case wgpu::TextureFormat::RG16Float:
    case wgpu::TextureFormat::RGBA16Float:
    case wgpu::TextureFormat::R32Float:
    case wgpu::TextureFormat::RG32Float:
    case wgpu::TextureFormat::RGBA32Float:
    case wgpu::TextureFormat::RG11B10Ufloat:
    case wgpu::TextureFormat::RGB10A2Unorm:
      return true;
    default:
      return false;
  }
}

bool IsFloatFormat(wgpu::TextureFormat format) {
  return format == wgpu::TextureFormat::RGBA16Float ||
         format == wgpu::TextureFormat::RGBA32Float ||
         format == wgpu::TextureFormat::RG11B10Ufloat ||
         format == wgpu::TextureFormat::RG16Float ||
         format == wgpu::TextureFormat::R32Float;
}

}
