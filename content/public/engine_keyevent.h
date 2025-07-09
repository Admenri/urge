// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_KEYEVENT_H_
#define CONTENT_PUBLIC_ENGINE_KEYEVENT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:KeyEvent)--*/
class URGE_OBJECT(KeyEvent) {
 public:
  virtual ~KeyEvent() = default;

  /*--urge(name:EventType)--*/
  enum Type {
    TYPE_KEY_DOWN = 0,
    TYPE_KEY_UP,
  };

  /*--urge(name:update)--*/
  static base::Vector<scoped_refptr<KeyEvent>> Update(
      ExecutionContext* execution_context,
      ExceptionState& exception_state);

  /*--urge(name:type)--*/
  virtual Type GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:device_id)--*/
  virtual int32_t GetDeviceID(ExceptionState& exception_state) = 0;

  /*--urge(name:scancode)--*/
  virtual int32_t GetScancode(ExceptionState& exception_state) = 0;

  /*--urge(name:key)--*/
  virtual int32_t GetKey(ExceptionState& exception_state) = 0;

  /*--urge(name:modifier)--*/
  virtual int32_t GetModifier(ExceptionState& exception_state) = 0;

  /*--urge(name:down?)--*/
  virtual bool GetDown(ExceptionState& exception_state) = 0;

  /*--urge(name:repeat?)--*/
  virtual bool GetRepeat(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_KEYEVENT_H_
