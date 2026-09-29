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

#include <functional>
#include <stack>

#include "core/bitmap.h"
#include "core/common.h"
#include "core/gpu.h"
#include "core/object.h"

namespace urge {

struct DrawContext {
  // Current context model transform stack
  std::stack<Mat4x4> model;

  // Command buffer
  wgpu::CommandEncoder command;

  // Render pass (drawing stage)
  wgpu::RenderPassEncoder pass;

  // Render target (drawing stage)
  RefPtr<Bitmap> target;

  // Scene bindgroup (drawing stage)
  wgpu::BindGroup scene;

  // Scissor stack (drawing stage)
  std::stack<RectI> scissors;
};
using DrawParam = DrawContext*;

struct ZValue {
  int32_t value = 0;
  int32_t sorting = 0;
  uint64_t timestamp = 0;
  static inline uint64_t counter = 0;

  ZValue() : timestamp(counter++) {}
  ZValue(int32_t v) : value(v), timestamp(counter++) {}
  ZValue(int32_t v, int32_t s) : value(v), sorting(s), timestamp(counter++) {}

  bool operator==(const ZValue& other) const {
    return value == other.value && sorting == other.sorting &&
           timestamp == other.timestamp;
  }

  bool operator<(const ZValue& other) const {
    if (value != other.value)
      return value < other.value;
    if (sorting != other.sorting)
      return sorting < other.sorting;
    return timestamp < other.timestamp;
  }

  bool operator>(const ZValue& other) const { return other < *this; }
  bool operator<=(const ZValue& other) const { return !(*this > other); }
  bool operator>=(const ZValue& other) const { return !(*this < other); }
};

class DrawableSet;

class Drawable {
 public:
  using OnPrepare = std::function<void(DrawParam)>;
  using OnDraw = std::function<void(DrawParam)>;

  Drawable();
  Drawable(OnPrepare on_prepare, OnDraw on_draw, const ZValue& order);
  ~Drawable();

  void SortWith(ZValue value);

  bool& visible() { return visible_; }
  ZValue& order() { return z_; }

  void SetParent(DrawableSet* parent);
  void RemoveFromList();

  Drawable* Prev() { return prev_; }
  Drawable* Next() { return next_; }

  template <typename Ty>
  void SetupTrait(Ty* self) {
    trait_type_ = TypeID::Of<Ty>();
    self_data_ = self;
  }

  template <typename Ty>
  Ty* TryCast() {
    if (TypeID::Of<Ty>() == trait_type_)
      return static_cast<Ty*>(self_data_);
    return nullptr;
  }

 private:
  friend class DrawableSet;

  void InsertAfter(Drawable* node);
  void Resort(ZValue old);
  void BubbleLeft();
  void BubbleRight();

  OnPrepare on_prepare_;
  OnDraw on_draw_;

  Drawable* prev_ = nullptr;
  Drawable* next_ = nullptr;

  DrawableSet* parent_ = nullptr;
  bool visible_ = true;
  ZValue z_ = {};

  size_t trait_type_ = 0;
  void* self_data_ = nullptr;
};

class DrawableSet {
 public:
  DrawableSet();
  ~DrawableSet();

  void DispatchPrepare(DrawParam param);
  void DispatchDraw(DrawParam param);

 private:
  friend class Drawable;
  Drawable root_;  // sentinel node for doubly-linked list
};

}  // namespace urge
