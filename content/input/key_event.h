// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_KEY_EVENT_H_
#define CONTENT_INPUT_KEY_EVENT_H_

#include "base/memory/ref_counted.h"
#include "content/public/engine_keyevent.h"
#include "content/worker/event_controller.h"

namespace content {

class KeyEventImpl : public KeyEvent {
 public:
  KeyEventImpl(EventController::KeyEventData event);
  ~KeyEventImpl() override;

  KeyEventImpl(const KeyEventImpl&) = delete;
  KeyEventImpl& operator=(const KeyEventImpl&) = delete;

 protected:
  Type GetType(ExceptionState& exception_state) override;
  int32_t GetDeviceID(ExceptionState& exception_state) override;
  int32_t GetScancode(ExceptionState& exception_state) override;
  int32_t GetKey(ExceptionState& exception_state) override;
  int32_t GetModifier(ExceptionState& exception_state) override;
  bool GetDown(ExceptionState& exception_state) override;
  bool GetRepeat(ExceptionState& exception_state) override;

 private:
  EventController::KeyEventData event_;
};

}  // namespace content

#endif  //! CONTENT_INPUT_KEY_EVENT_H_
