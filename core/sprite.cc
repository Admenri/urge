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

#include "core/sprite.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "core/config.h"

namespace urge {

namespace {

// Legacy PI macro value from the raylib headers.
constexpr float kPi = 3.14159265358979323846f;

}  // namespace

Sprite::Sprite(RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()),
      src_rect_(MakeRefCounted<Rect>()),
      // The default attribute values of RGSS: no color and no tone blend
      color_(MakeRefCounted<Color>(0.f, 0.f, 0.f, 0.f)),
      tone_(MakeRefCounted<Tone>(0.f, 0.f, 0.f, 0.f)),
      rgssvx_style_(Config::Get().vx() || Config::Get().vxa()) {
  Node::SetupTrait(this);
}

Sprite::~Sprite() {
  Disposable::Dispose();
}

void Sprite::Flash(RefPtr<Color> color, int32_t duration) {
  flash_.color = color ? color->Normalize() : Vec4{};
  flash_.step = duration > 0 ? (flash_.color.w / duration) : 0.0f;
}

void Sprite::Update() {
  wave_phase_ += wave_speed_ / 180.0f;
  wave_phase_ = std::fmod(wave_phase_, 360.0f);

  flash_.color.w -= flash_.step;
  if (flash_.color.w <= 0) {
    flash_.color = {};
    flash_.step = 0.0f;
  }
}

int32_t Sprite::GetWidth() {
  return src_rect_->data.width;
}

int32_t Sprite::GetHeight() {
  return src_rect_->data.height;
}

ATTR_DEF(Sprite, RefPtr<Viewport>, Viewport) {
  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(Sprite, RefPtr<Bitmap>, Bitmap) {
  if (value.has_value()) {
    bitmap_ = *value;
    if (bitmap_)
      src_rect_->Set(bitmap_->GetRect());
    return std::nullopt;
  } else {
    return bitmap_;
  }
}

ATTR_DEF(Sprite, RefPtr<Rect>, SrcRect) {
  if (value.has_value()) {
    src_rect_->Set(*value);
    return std::nullopt;
  } else {
    return src_rect_;
  }
}

ATTR_DEF(Sprite, int32_t, X) {
  if (value.has_value()) {
    x_ = *value;
    return std::nullopt;
  } else {
    return x_;
  }
}

ATTR_DEF(Sprite, int32_t, Y) {
  if (value.has_value()) {
    y_ = *value;
    if (rgssvx_style_) {
      ZValue zvalue = Node::GetOrder();
      zvalue.sorting = y_;
      Node::SortWith(zvalue);
    }
    return std::nullopt;
  } else {
    return y_;
  }
}

ATTR_DEF(Sprite, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(Sprite, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

ATTR_DEF(Sprite, float, ZoomX) {
  if (value.has_value()) {
    zoom_x_ = *value;
    return std::nullopt;
  } else {
    return zoom_x_;
  }
}

ATTR_DEF(Sprite, float, ZoomY) {
  if (value.has_value()) {
    zoom_y_ = *value;
    return std::nullopt;
  } else {
    return zoom_y_;
  }
}

ATTR_DEF(Sprite, float, Angle) {
  if (value.has_value()) {
    angle_ = *value;
    return std::nullopt;
  } else {
    return angle_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveAmp) {
  if (value.has_value()) {
    wave_amp_ = *value;
    return std::nullopt;
  } else {
    return wave_amp_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveLength) {
  if (value.has_value()) {
    wave_length_ = *value;
    return std::nullopt;
  } else {
    return wave_length_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveSpeed) {
  if (value.has_value()) {
    wave_speed_ = *value;
    return std::nullopt;
  } else {
    return wave_speed_;
  }
}

ATTR_DEF(Sprite, float, WavePhase) {
  if (value.has_value()) {
    wave_phase_ = *value;
    return std::nullopt;
  } else {
    return wave_phase_;
  }
}

ATTR_DEF(Sprite, bool, Mirror) {
  if (value.has_value()) {
    mirror_ = *value;
    return std::nullopt;
  } else {
    return mirror_;
  }
}

ATTR_DEF(Sprite, int32_t, BushDepth) {
  if (value.has_value()) {
    bush_depth_ = *value;
    return std::nullopt;
  } else {
    return bush_depth_;
  }
}

ATTR_DEF(Sprite, int32_t, BushOpacity) {
  if (value.has_value()) {
    bush_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return bush_opacity_;
  }
}

ATTR_DEF(Sprite, int32_t, Opacity) {
  if (value.has_value()) {
    opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return opacity_;
  }
}

ATTR_DEF(Sprite, int32_t, BlendType) {
  if (value.has_value()) {
    blend_type_ = *value;
    return std::nullopt;
  } else {
    return blend_type_;
  }
}

ATTR_DEF(Sprite, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_->Set(*value);
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Sprite, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

void Sprite::DisposeObject() {
  bitmap_.reset();
}

void Sprite::Prepare(DrawParam param) {}

void Sprite::DoDraw(DrawParam param) {}

}  // namespace urge
