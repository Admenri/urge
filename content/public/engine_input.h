// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_INPUT_H_
#define CONTENT_PUBLIC_ENGINE_INPUT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
// Interface reference: RGSS Reference
/*--urge(name:Input,is_module)--*/
class URGE_RUNTIME_API Input : public base::RefCounted<Input> {
 public:
  virtual ~Input() = default;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  virtual bool IsPressed(const base::String& sym,
                         ExceptionState& exception_state) = 0;
  virtual bool IsTriggered(const base::String& sym,
                           ExceptionState& exception_state) = 0;
  virtual bool IsRepeated(const base::String& sym,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:dir4)--*/
  virtual int32_t Dir4(ExceptionState& exception_state) = 0;

  /*--urge(name:dir8)--*/
  virtual int32_t Dir8(ExceptionState& exception_state) = 0;

  /*--urge(name:press_key?)--*/
  virtual bool KeyPressed(int32_t scancode,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:trigger_key?)--*/
  virtual bool KeyTriggered(int32_t scancode,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:repeat_key?)--*/
  virtual bool KeyRepeated(int32_t scancode,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:get_key_name)--*/
  virtual base::String GetKeyName(int32_t scancode,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:get_keys_from_flag)--*/
  virtual base::Vector<int32_t> GetKeysFromFlag(
      const base::String& flag,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_keys_from_flag)--*/
  virtual void SetKeysFromFlag(const base::String& flag,
                               const base::Vector<int32_t>& keys,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:recent_pressed_keys)--*/
  virtual base::Vector<int32_t> GetRecentPressed(
      ExceptionState& exception_state) = 0;

  /*--urge(name:recent_triggered_keys)--*/
  virtual base::Vector<int32_t> GetRecentTriggered(
      ExceptionState& exception_state) = 0;

  /*--urge(name:recent_repeated_keys)--*/
  virtual base::Vector<int32_t> GetRecentRepeated(
      ExceptionState& exception_state) = 0;

  /*--urge(name:emulate,optional:modifier=0,optional:repeat=false)--*/
  virtual bool Emulate(int32_t scancode,
                       bool down,
                       int32_t modifier,
                       bool repeat,
                       ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_INPUT_H_
