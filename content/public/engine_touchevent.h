// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_TOUCHEVENT_H_
#define CONTENT_PUBLIC_ENGINE_TOUCHEVENT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:TouchEvent)--*/
class URGE_RUNTIME_API TouchEvent : public base::RefCounted<TouchEvent> {
 public:
  virtual ~TouchEvent() = default;

  /*--urge(name:EventType)--*/
  enum Type {
    TYPE_FINGER_DOWN = 0,
    TYPE_FINGER_UP,
    TYPE_FINGER_MOTION,
    TYPE_FINGER_CANCELED,
  };

  /*--urge(name:update)--*/
  static base::Vector<scoped_refptr<TouchEvent>> Update(
      ExecutionContext* execution_context,
      ExceptionState& exception_state);

  /*--urge(name:type)--*/
  virtual Type GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:device_id)--*/
  virtual int32_t GetDeviceID(ExceptionState& exception_state) = 0;

  /*--urge(name:finger)--*/
  virtual int32_t GetFinger(ExceptionState& exception_state) = 0;

  /*--urge(name:x)--*/
  virtual int32_t GetX(ExceptionState& exception_state) = 0;

  /*--urge(name:y)--*/
  virtual int32_t GetY(ExceptionState& exception_state) = 0;

  /*--urge(name:dx)--*/
  virtual int32_t GetDeltaX(ExceptionState& exception_state) = 0;

  /*--urge(name:dy)--*/
  virtual int32_t GetDeltaY(ExceptionState& exception_state) = 0;

  /*--urge(name:pressure)--*/
  virtual float GetPressure(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_TOUCHEVENT_H_
