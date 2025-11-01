// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/color_impl.h"

#include <algorithm>

namespace content {

// static
scoped_refptr<Color> Color::New(ExecutionContext* execution_context,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<ColorImpl>(base::Vec4(0));
}

// static
scoped_refptr<Color> Color::New(ExecutionContext* execution_context,
                                float red,
                                float green,
                                float blue,
                                float alpha,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<ColorImpl>(base::Vec4(red, green, blue, alpha));
}

// static
scoped_refptr<Color> Color::New(ExecutionContext* execution_context,
                                float red,
                                float green,
                                float blue,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<ColorImpl>(base::Vec4(red, green, blue, 255.0f));
}

// static
scoped_refptr<Color> Color::Copy(ExecutionContext* execution_context,
                                 scoped_refptr<Color> other,
                                 ExceptionState& exception_state) {
  return base::MakeRefCounted<ColorImpl>(*static_cast<ColorImpl*>(other.get()));
}

// static
scoped_refptr<Color> Color::Deserialize(ExecutionContext* execution_context,
                                        const std::string& data,
                                        ExceptionState& exception_state) {
  if (data.size() < sizeof(double) * 4) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid data length, size: %d", data.size());
    return nullptr;
  }

  const double* ptr = reinterpret_cast<const double*>(data.data());
  const float red = static_cast<float>(*(ptr + 0));
  const float green = static_cast<float>(*(ptr + 1));
  const float blue = static_cast<float>(*(ptr + 2));
  const float alpha = static_cast<float>(*(ptr + 3));

  return base::MakeRefCounted<ColorImpl>(base::Vec4(red, green, blue, alpha));
}

// static
std::string Color::Serialize(ExecutionContext* execution_context,
                             scoped_refptr<Color> value,
                             ExceptionState& exception_state) {
  scoped_refptr<ColorImpl> impl = ColorImpl::From(value);
  std::string serial_data(sizeof(double) * 4, 0);

  double* target_ptr = reinterpret_cast<double*>(serial_data.data());
  *(target_ptr + 0) = impl->value_.x;
  *(target_ptr + 1) = impl->value_.y;
  *(target_ptr + 2) = impl->value_.z;
  *(target_ptr + 3) = impl->value_.w;

  return serial_data;
}

///////////////////////////////////////////////////////////////////////////////
// ColorImpl Implement

ColorImpl::ColorImpl(const base::Vec4& value) : value_(value), dirty_(true) {
  value_.x = std::clamp(value_.x, 0.0f, 255.0f);
  value_.y = std::clamp(value_.y, 0.0f, 255.0f);
  value_.z = std::clamp(value_.z, 0.0f, 255.0f);
  value_.w = std::clamp(value_.w, 0.0f, 255.0f);
}

ColorImpl::ColorImpl(const ColorImpl& other)
    : value_(other.value_), dirty_(true) {}

ColorImpl& ColorImpl::operator=(const ColorImpl& other) {
  value_ = other.value_;
  NotifyObservers();
  return *this;
}

scoped_refptr<ColorImpl> ColorImpl::From(scoped_refptr<Color> host) {
  return static_cast<ColorImpl*>(host.get());
}

bool ColorImpl::CompareWithOther(scoped_refptr<Color> other,
                                 ExceptionState& exception_state) {
  if (!other)
    return false;

  return static_cast<ColorImpl*>(other.get())->value_ == value_;
}

void ColorImpl::Set(float red,
                    float green,
                    float blue,
                    float alpha,
                    ExceptionState& exception_state) {
  value_.x = std::clamp(red, 0.0f, 255.0f);
  value_.y = std::clamp(green, 0.0f, 255.0f);
  value_.z = std::clamp(blue, 0.0f, 255.0f);
  value_.w = std::clamp(alpha, 0.0f, 255.0f);
  NotifyObservers();
}

void ColorImpl::Set(float red,
                    float green,
                    float blue,
                    ExceptionState& exception_state) {
  Set(red, green, blue, 255.0f, exception_state);
}

void ColorImpl::Set(scoped_refptr<Color> other,
                    ExceptionState& exception_state) {
  if (!other)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid value");

  value_ = static_cast<ColorImpl*>(other.get())->value_;
  NotifyObservers();
}

SDL_Color ColorImpl::AsSDLColor() {
  return {static_cast<uint8_t>(value_.x), static_cast<uint8_t>(value_.y),
          static_cast<uint8_t>(value_.z), static_cast<uint8_t>(value_.w)};
}

base::Vec4 ColorImpl::AsNormColor() {
  if (dirty_) {
    dirty_ = false;
    norm_.x = value_.x / 255.0f;
    norm_.y = value_.y / 255.0f;
    norm_.z = value_.z / 255.0f;
    norm_.w = value_.w / 255.0f;
  }

  return norm_;
}

void ColorImpl::NotifyObservers() {
  ValueNotification::NotifyObservers();
  dirty_ = true;
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Red,
    float,
    ColorImpl,
    { return value_.x; },
    {
      if (value_.x != value) {
        value_.x = std::clamp(value, 0.0f, 255.0f);
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Green,
    float,
    ColorImpl,
    { return value_.y; },
    {
      if (value_.y != value) {
        value_.y = std::clamp(value, 0.0f, 255.0f);
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Blue,
    float,
    ColorImpl,
    { return value_.z; },
    {
      if (value_.z != value) {
        value_.z = std::clamp(value, 0.0f, 255.0f);
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Alpha,
    float,
    ColorImpl,
    { return value_.w; },
    {
      if (value_.w != value) {
        value_.w = std::clamp(value, 0.0f, 255.0f);
        NotifyObservers();
      }
    });

}  // namespace content
