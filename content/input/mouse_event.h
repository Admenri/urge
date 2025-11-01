// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_MOUSE_EVENT_H_
#define CONTENT_INPUT_MOUSE_EVENT_H_

#include "base/memory/ref_counted.h"
#include "content/public/engine_mouseevent.h"
#include "content/worker/event_controller.h"

namespace content {

class MouseEventImpl : public MouseEvent {
 public:
  MouseEventImpl(EventController::MouseEventData event);
  ~MouseEventImpl() override;

  MouseEventImpl(const MouseEventImpl&) = delete;
  MouseEventImpl& operator=(const MouseEventImpl&) = delete;

 protected:
  Type GetType(ExceptionState& exception_state) override;
  int32_t GetMouseID(ExceptionState& exception_state) override;
  int32_t GetX(ExceptionState& exception_state) override;
  int32_t GetY(ExceptionState& exception_state) override;
  int32_t GetButton(ExceptionState& exception_state) override;
  bool GetButtonDown(ExceptionState& exception_state) override;
  int32_t GetButtonClicks(ExceptionState& exception_state) override;
  int32_t GetMotion(ExceptionState& exception_state) override;
  int32_t GetMotionX(ExceptionState& exception_state) override;
  int32_t GetMotionY(ExceptionState& exception_state) override;
  WheelState GetWheel(ExceptionState& exception_state) override;
  int32_t GetWheelX(ExceptionState& exception_state) override;
  int32_t GetWheelY(ExceptionState& exception_state) override;

 private:
  EventController::MouseEventData event_;
};

}  // namespace content

#endif  //! CONTENT_INPUT_MOUSE_EVENT_H_
