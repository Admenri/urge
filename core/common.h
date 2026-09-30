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

#include <cmath>

#include "SDL3/SDL_rect.h"

#include "glm/glm.hpp"

namespace urge {

class RectI {
 public:
  RectI() : x(0), y(0), width(0), height(0) {}
  RectI(const glm::ivec2& size) : x(0), y(0), width(size.x), height(size.y) {}
  RectI(int32_t ix, int32_t iy, int32_t iw, int32_t ih)
      : x(ix), y(iy), width(iw), height(ih) {}
  RectI(const glm::ivec2& pos, const glm::ivec2& size)
      : x(pos.x), y(pos.y), width(size.x), height(size.y) {}

  RectI(const RectI& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
  }

  bool operator()() const { return width && height; }

  RectI& operator=(const RectI& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
    return *this;
  }

  bool operator==(const RectI& other) const {
    return other.x == x && other.y == y && other.width == width &&
           other.height == height;
  }

  bool operator!=(const RectI& other) const {
    return other.x != x || other.y != y || other.width != width ||
           other.height != height;
  }

  RectI operator*(const RectI& value) const {
    return RectI(x * value.x, y * value.y, width * value.width,
                 height * value.height);
  }

  SDL_Rect ToSDLRect() const { return SDL_Rect{x, y, width, height}; }

  glm::ivec2 Position() const { return glm::ivec2(x, y); }
  glm::ivec2 Size() const { return glm::ivec2(width, height); }

 public:
  int32_t x, y, width, height;
};

class RectF {
 public:
  RectF() : x(0.f), y(0.f), width(0.f), height(0.f) {}
  RectF(const glm::vec2& size) : x(0), y(0), width(size.x), height(size.y) {}
  RectF(float ix, float iy, float iw, float ih)
      : x(ix), y(iy), width(iw), height(ih) {}
  RectF(const glm::vec2& pos, const glm::vec2& size)
      : x(pos.x), y(pos.y), width(size.x), height(size.y) {}
  RectF(const RectI& ir)
      : x(static_cast<float>(ir.x)),
        y(static_cast<float>(ir.y)),
        width(static_cast<float>(ir.width)),
        height(static_cast<float>(ir.height)) {}

  RectF(const RectF& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
  }

  bool operator()() const { return width && height; }

  RectF& operator=(const RectF& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
    return *this;
  }

  bool operator==(const RectF& other) const {
    return other.x == x && other.y == y && other.width == width &&
           other.height == height;
  }

  bool operator!=(const RectF& other) const {
    return other.x != x || other.y != y || other.width != width ||
           other.height != height;
  }

  RectF operator*(const RectF& value) const {
    return RectF(x * value.x, y * value.y, width * value.width,
                 height * value.height);
  }

  glm::vec2 Position() const { return glm::vec2(x, y); }
  glm::vec2 Size() const { return glm::vec2(width, height); }

 public:
  float x, y, width, height;
};

inline RectF MakeNorm(RectF rect, glm::vec2 size) {
  RectF result = rect;
  result.x /= size.x;
  result.y /= size.y;
  result.width /= size.x;
  result.height /= size.y;
  return result;
}

inline RectI MakeIntersect(const RectI& A, const RectI& B) {
  int32_t Amin, Amax, Bmin, Bmax;
  RectI result;

  // Horizontal intersection
  Amin = A.x;
  Amax = Amin + A.width;
  Bmin = B.x;
  Bmax = Bmin + B.width;
  if (Bmin > Amin) {
    Amin = Bmin;
  }
  result.x = Amin;
  if (Bmax < Amax) {
    Amax = Bmax;
  }
  result.width = std::max(0, Amax - Amin);

  // Vertical intersection
  Amin = A.y;
  Amax = Amin + A.height;
  Bmin = B.y;
  Bmax = Bmin + B.height;
  if (Bmin > Amin) {
    Amin = Bmin;
  }
  result.y = Amin;
  if (Bmax < Amax) {
    Amax = Bmax;
  }
  result.height = std::max(0, Amax - Amin);

  return result;
}

inline glm::ivec2 ExtractPosition(const glm::mat4& matrix) {
  return glm::ivec2(static_cast<int32_t>(std::lround(matrix[3].x)),
                    static_cast<int32_t>(std::lround(matrix[3].y)));
}

}  // namespace urge
