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

#include "core/viewport.h"

#include "core/graphics.h"

namespace urge {

Viewport::Viewport(int32_t x, int32_t y, int32_t width, int32_t height)
    : Node(nullptr, ZValue()),
      rect_(MakeRefCounted<Rect>(x, y, width, height)),
      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()) {
  Node::SetupTrait(this);
}

Viewport::Viewport(RefPtr<Rect> rect)
    : Viewport(rect->data.x,
               rect->data.y,
               rect->data.width,
               rect->data.height) {}

Viewport::Viewport()
    : Viewport(0, 0, Graphics::Get().GetWidth(), Graphics::Get().GetHeight()) {}

Viewport::~Viewport() {
  Disposable::Dispose();
}

void Viewport::Flash(RefPtr<Color> color, int32_t duration) {
  flash_.color = color ? color->Normalize() : Vec4();
  flash_.step =
      duration > 0 ? (flash_.color.w / static_cast<float>(duration)) : 0.0f;
}

void Viewport::Update() {
  flash_.color.w -= flash_.step;
  if (flash_.color.w <= 0) {
    flash_.color = {};
    flash_.step = 0.0f;
  }
}

ATTR_DEF(Viewport, RefPtr<Rect>, Rect) {
  if (value.has_value()) {
    rect_->Set(*value);
    return std::nullopt;
  } else {
    return rect_;
  }
}

ATTR_DEF(Viewport, int32_t, OX) {
  if (value.has_value()) {
    origin_.x = *value;
    return std::nullopt;
  } else {
    return origin_.x;
  }
}

ATTR_DEF(Viewport, int32_t, OY) {
  if (value.has_value()) {
    origin_.y = *value;
    return std::nullopt;
  } else {
    return origin_.y;
  }
}

ATTR_DEF(Viewport, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_->Set(*value);
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Viewport, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

void Viewport::DisposeObject() {}

void Viewport::Prepare(DrawParam param) {}

void Viewport::DoDraw(DrawParam param) {}

void Viewport::PostDraw(DrawParam param) {}

}  // namespace urge
