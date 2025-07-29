// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_MOUSEEVENT_H_
#define CONTENT_PUBLIC_ENGINE_MOUSEEVENT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:MouseEvent)--*/
class URGE_OBJECT(MouseEvent) {
 public:
  virtual ~MouseEvent() = default;

  /*--urge(name:EventType)--*/
  enum Type {
    TYPE_MOUSE_MOTION = 0,
    TYPE_MOUSE_BUTTON_DOWN,
    TYPE_MOUSE_BUTTON_UP,
    TYPE_MOUSE_WHEEL,
  };

  /*--urge(name:WheelState)--*/
  enum WheelState {
    MOUSEWHEEL_NORMAL = 0,
    MOUSEWHEEL_FLIPPED,
  };

  /*--urge(name:update)--*/
  static std::vector<scoped_refptr<MouseEvent>> Update(
      ExecutionContext* execution_context,
      ExceptionState& exception_state);

  /*--urge(name:type)--*/
  virtual Type GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:mouse_id)--*/
  virtual int32_t GetMouseID(ExceptionState& exception_state) = 0;

  /*--urge(name:x)--*/
  virtual int32_t GetX(ExceptionState& exception_state) = 0;

  /*--urge(name:y)--*/
  virtual int32_t GetY(ExceptionState& exception_state) = 0;

  /*--urge(name:button)--*/
  virtual int32_t GetButton(ExceptionState& exception_state) = 0;

  /*--urge(name:button_down?)--*/
  virtual bool GetButtonDown(ExceptionState& exception_state) = 0;

  /*--urge(name:button_clicks)--*/
  virtual int32_t GetButtonClicks(ExceptionState& exception_state) = 0;

  /*--urge(name:motion)--*/
  virtual int32_t GetMotion(ExceptionState& exception_state) = 0;

  /*--urge(name:motion_x)--*/
  virtual int32_t GetMotionX(ExceptionState& exception_state) = 0;

  /*--urge(name:motion_y)--*/
  virtual int32_t GetMotionY(ExceptionState& exception_state) = 0;

  /*--urge(name:wheel)--*/
  virtual WheelState GetWheel(ExceptionState& exception_state) = 0;

  /*--urge(name:wheel_x)--*/
  virtual int32_t GetWheelX(ExceptionState& exception_state) = 0;

  /*--urge(name:wheel_y)--*/
  virtual int32_t GetWheelY(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_MOUSEEVENT_H_
