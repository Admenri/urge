// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_INPUT_H_
#define CONTENT_PUBLIC_ENGINE_INPUT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:Input,is_module)--*/
class URGE_OBJECT(Input) {
 public:
  virtual ~Input() = default;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  virtual bool IsPressed(const std::string& sym,
                         ExceptionState& exception_state) = 0;
  virtual bool IsTriggered(const std::string& sym,
                           ExceptionState& exception_state) = 0;
  virtual bool IsRepeated(const std::string& sym,
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
  virtual std::string GetKeyName(int32_t scancode,
                                  ExceptionState& exception_state) = 0;

  /*--urge(name:get_keys_from_flag)--*/
  virtual std::vector<int32_t> GetKeysFromFlag(
      const std::string& flag,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_keys_from_flag)--*/
  virtual void SetKeysFromFlag(const std::string& flag,
                               const std::vector<int32_t>& keys,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:recent_pressed_keys)--*/
  virtual std::vector<int32_t> GetRecentPressed(
      ExceptionState& exception_state) = 0;

  /*--urge(name:recent_triggered_keys)--*/
  virtual std::vector<int32_t> GetRecentTriggered(
      ExceptionState& exception_state) = 0;

  /*--urge(name:recent_repeated_keys)--*/
  virtual std::vector<int32_t> GetRecentRepeated(
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
