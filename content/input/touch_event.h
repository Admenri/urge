// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_TOUCH_EVENT_H_
#define CONTENT_INPUT_TOUCH_EVENT_H_

#include "base/memory/ref_counted.h"
#include "content/public/engine_touchevent.h"
#include "content/worker/event_controller.h"

namespace content {

class TouchEventImpl : public TouchEvent {
 public:
  TouchEventImpl(EventController::TouchEventData event);
  ~TouchEventImpl() override;

  TouchEventImpl(const TouchEventImpl&) = delete;
  TouchEventImpl& operator=(const TouchEventImpl&) = delete;

 protected:
  Type GetType(ExceptionState& exception_state) override;
  int32_t GetDeviceID(ExceptionState& exception_state) override;
  int32_t GetFinger(ExceptionState& exception_state) override;
  int32_t GetX(ExceptionState& exception_state) override;
  int32_t GetY(ExceptionState& exception_state) override;
  int32_t GetDeltaX(ExceptionState& exception_state) override;
  int32_t GetDeltaY(ExceptionState& exception_state) override;
  float GetPressure(ExceptionState& exception_state) override;

 private:
  EventController::TouchEventData event_;
};

}  // namespace content

#endif  // !CONTENT_INPUT_TOUCH_EVENT_H_
