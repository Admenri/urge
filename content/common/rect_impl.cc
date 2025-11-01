// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/rect_impl.h"

namespace content {

// static
scoped_refptr<Rect> Rect::New(ExecutionContext* execution_context,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<RectImpl>(base::Rect());
}

// static
scoped_refptr<Rect> Rect::New(ExecutionContext* execution_context,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<RectImpl>(base::Rect(x, y, width, height));
}

// static
scoped_refptr<Rect> Rect::Copy(ExecutionContext* execution_context,
                               scoped_refptr<Rect> other,
                               ExceptionState& exception_state) {
  return base::MakeRefCounted<RectImpl>(*static_cast<RectImpl*>(other.get()));
}

// static
scoped_refptr<Rect> Rect::Deserialize(ExecutionContext* execution_context,
                                      const std::string& data,
                                      ExceptionState& exception_state) {
  if (data.size() < sizeof(int32_t) * 4) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid data length, size: %d", data.size());
    return nullptr;
  }

  const int32_t* ptr = reinterpret_cast<const int32_t*>(data.data());
  return base::MakeRefCounted<RectImpl>(
      base::Rect(*(ptr + 0), *(ptr + 1), *(ptr + 2), *(ptr + 3)));
}

// static
std::string Rect::Serialize(ExecutionContext* execution_context,
                            scoped_refptr<Rect> value,
                            ExceptionState& exception_state) {
  RectImpl* impl = static_cast<RectImpl*>(value.get());
  std::string serial_data(sizeof(base::Rect), 0);
  std::memcpy(serial_data.data(), &impl->rect_, sizeof(base::Rect));
  return serial_data;
}

///////////////////////////////////////////////////////////////////////////////
// RectImpl Implement

RectImpl::RectImpl(const base::Rect& rect) : rect_(rect) {}

RectImpl::RectImpl(const RectImpl& other) : rect_(other.rect_) {}

RectImpl& RectImpl::operator=(const RectImpl& other) {
  rect_ = other.rect_;
  NotifyObservers();
  return *this;
}

scoped_refptr<RectImpl> RectImpl::From(scoped_refptr<Rect> host) {
  return static_cast<RectImpl*>(host.get());
}

bool RectImpl::CompareWithOther(scoped_refptr<Rect> other,
                                ExceptionState& exception_state) {
  if (!other)
    return false;

  return static_cast<RectImpl*>(other.get())->rect_ == rect_;
}

void RectImpl::Set(int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height,
                   ExceptionState& exception_state) {
  rect_ = base::Rect(x, y, width, height);
  NotifyObservers();
}

void RectImpl::Set(scoped_refptr<Rect> other, ExceptionState& exception_state) {
  if (!other)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid value");

  rect_ = static_cast<RectImpl*>(other.get())->rect_;
  NotifyObservers();
}

void RectImpl::Empty(ExceptionState& exception_state) {
  rect_ = base::Rect();
  NotifyObservers();
}

void RectImpl::SetBase(const base::Rect& base) {
  rect_ = base;
  NotifyObservers();
}

base::Rect RectImpl::AsBaseRect() {
  base::Rect tmp(rect_);
  tmp.width = std::max(0, tmp.width);
  tmp.height = std::max(0, tmp.height);
  return tmp;
}

SDL_Rect RectImpl::AsSDLRect() {
  return SDL_Rect{rect_.x, rect_.y, rect_.width, rect_.height};
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    X,
    int32_t,
    RectImpl,
    { return rect_.x; },
    {
      if (rect_.x != value) {
        rect_.x = value;
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Y,
    int32_t,
    RectImpl,
    { return rect_.y; },
    {
      if (rect_.y != value) {
        rect_.y = value;
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Width,
    int32_t,
    RectImpl,
    { return rect_.width; },
    {
      if (rect_.width != value) {
        rect_.width = value;
        NotifyObservers();
      }
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Height,
    int32_t,
    RectImpl,
    { return rect_.height; },
    {
      if (rect_.height != value) {
        rect_.height = value;
        NotifyObservers();
      }
    });

}  // namespace content
