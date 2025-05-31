// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_SPINEEVENT_H_
#define CONTENT_PUBLIC_ENGINE_SPINEEVENT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:SpineEvent)--*/
class URGE_RUNTIME_API SpineEvent : public base::RefCounted<SpineEvent> {
 public:
  virtual ~SpineEvent() = default;

  /*--urge(name:Type)--*/
  enum Type {
    EVENT_START = 0,
    EVENT_INTERRUPT,
    EVENT_END,
    EVENT_COMPLETE,
    EVENT_DISPOSE,
    EVENT_EVENT,
  };

  /*--urge(name:type)--*/
  virtual Type GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:name)--*/
  virtual base::String GetName(ExceptionState& exception_state) = 0;

  /*--urge(name:track_index)--*/
  virtual int32_t GetTrackIndex(ExceptionState& exception_state) = 0;

  /*--urge(name:time)--*/
  virtual float GetTime(ExceptionState& exception_state) = 0;

  /*--urge(name:int_value)--*/
  virtual int32_t GetIntValue(ExceptionState& exception_state) = 0;

  /*--urge(name:float_value)--*/
  virtual float GetFloatValue(ExceptionState& exception_state) = 0;

  /*--urge(name:string_value)--*/
  virtual base::String GetStringValue(ExceptionState& exception_state) = 0;

  /*--urge(name:volume)--*/
  virtual float GetVolume(ExceptionState& exception_state) = 0;

  /*--urge(name:balance)--*/
  virtual float GetBalance(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_SPINEEVENT_H_
