// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/tone_impl.h"

#include <algorithm>

namespace content {

scoped_refptr<Tone> Tone::New(ExecutionContext* execution_context,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<ToneImpl>(base::Vec4(0));
}

scoped_refptr<Tone> Tone::New(ExecutionContext* execution_context,
                              float red,
                              float green,
                              float blue,
                              float gray,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<ToneImpl>(base::Vec4(red, green, blue, gray));
}

scoped_refptr<Tone> Tone::New(ExecutionContext* execution_context,
                              float red,
                              float green,
                              float blue,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<ToneImpl>(base::Vec4(red, green, blue, 0.0f));
}

scoped_refptr<Tone> Tone::Copy(ExecutionContext* execution_context,
                               scoped_refptr<Tone> other,
                               ExceptionState& exception_state) {
  return base::MakeRefCounted<ToneImpl>(*static_cast<ToneImpl*>(other.get()));
}

scoped_refptr<Tone> Tone::Deserialize(ExecutionContext* execution_context,
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
  const float gray = static_cast<float>(*(ptr + 3));

  return base::MakeRefCounted<ToneImpl>(base::Vec4(red, green, blue, gray));
}

std::string Tone::Serialize(ExecutionContext* execution_context,
                            scoped_refptr<Tone> value,
                            ExceptionState& exception_state) {
  scoped_refptr<ToneImpl> impl = ToneImpl::From(value);
  std::string serial_data(sizeof(double) * 4, 0);

  double* target_ptr = reinterpret_cast<double*>(serial_data.data());
  *(target_ptr + 0) = impl->value_.x;
  *(target_ptr + 1) = impl->value_.y;
  *(target_ptr + 2) = impl->value_.z;
  *(target_ptr + 3) = impl->value_.w;

  return serial_data;
}

ToneImpl::ToneImpl(const base::Vec4& value) : value_(value), dirty_(true) {
  value_.x = std::clamp(value_.x, -255.0f, 255.0f);
  value_.y = std::clamp(value_.y, -255.0f, 255.0f);
  value_.z = std::clamp(value_.z, -255.0f, 255.0f);
  value_.w = std::clamp(value_.w, 0.0f, 255.0f);
}

ToneImpl::ToneImpl(const ToneImpl& other)
    : value_(other.value_), dirty_(true) {}

ToneImpl& ToneImpl::operator=(const ToneImpl& other) {
  value_ = other.value_;
  NotifyObservers();
  return *this;
}

scoped_refptr<ToneImpl> ToneImpl::From(scoped_refptr<Tone> host) {
  return static_cast<ToneImpl*>(host.get());
}

bool ToneImpl::CompareWithOther(scoped_refptr<Tone> other,
                                ExceptionState& exception_state) {
  if (!other)
    return false;

  return static_cast<ToneImpl*>(other.get())->value_ == value_;
}

void ToneImpl::Set(float red,
                   float green,
                   float blue,
                   float gray,
                   ExceptionState& exception_state) {
  value_.x = std::clamp(red, -255.0f, 255.0f);
  value_.y = std::clamp(green, -255.0f, 255.0f);
  value_.z = std::clamp(blue, -255.0f, 255.0f);
  value_.w = std::clamp(gray, 0.0f, 255.0f);
  NotifyObservers();
}

void ToneImpl::Set(float red,
                   float green,
                   float blue,
                   ExceptionState& exception_state) {
  Set(red, green, blue, 0.0f, exception_state);
}

void ToneImpl::Set(scoped_refptr<Tone> other, ExceptionState& exception_state) {
  if (!other)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid value");

  value_ = static_cast<ToneImpl*>(other.get())->value_;
  NotifyObservers();
}

base::Vec4 ToneImpl::AsNormColor() {
  if (dirty_) {
    dirty_ = false;
    norm_.x = value_.x / 255.0f;
    norm_.y = value_.y / 255.0f;
    norm_.z = value_.z / 255.0f;
    norm_.w = value_.w / 255.0f;
  }

  return norm_;
}

void ToneImpl::NotifyObservers() {
  ValueNotification::NotifyObservers();
  dirty_ = true;
}

float ToneImpl::Get_Red(ExceptionState& exception_state) {
  return value_.x;
}

void ToneImpl::Put_Red(const float& value, ExceptionState& exception_state) {
  if (value_.x != value) {
    value_.x = std::clamp(value, -255.0f, 255.0f);
    NotifyObservers();
  }
}

float ToneImpl::Get_Green(ExceptionState& exception_state) {
  return value_.y;
}

void ToneImpl::Put_Green(const float& value, ExceptionState& exception_state) {
  if (value_.y != value) {
    value_.y = std::clamp(value, -255.0f, 255.0f);
    NotifyObservers();
  }
}

float ToneImpl::Get_Blue(ExceptionState& exception_state) {
  return value_.z;
}

void ToneImpl::Put_Blue(const float& value, ExceptionState& exception_state) {
  if (value_.z != value) {
    value_.z = std::clamp(value, -255.0f, 255.0f);
    NotifyObservers();
  }
}

float ToneImpl::Get_Gray(ExceptionState& exception_state) {
  return value_.w;
}

void ToneImpl::Put_Gray(const float& value, ExceptionState& exception_state) {
  if (value_.w != value) {
    value_.w = std::clamp(value, 0.0f, 255.0f);
    NotifyObservers();
  }
}

}  // namespace content
