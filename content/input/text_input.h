// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_TEXT_INPUT_H_
#define CONTENT_INPUT_TEXT_INPUT_H_

#include "content/public/engine_textinputevent.h"
#include "content/worker/event_controller.h"

namespace content {

class TextInputEventImpl : public TextInputEvent {
 public:
  TextInputEventImpl(EventController::TextInputEventData event);
  ~TextInputEventImpl() override;

  TextInputEventImpl(const TextInputEventImpl&) = delete;
  TextInputEventImpl& operator=(const TextInputEventImpl&) = delete;

  Type GetType(ExceptionState& exception_state) override;
  std::string GetText(ExceptionState& exception_state) override;
  int32_t GetEditingStart(ExceptionState& exception_state) override;
  int32_t GetEditingLength(ExceptionState& exception_state) override;

 private:
  EventController::TextInputEventData event_;
};

}  // namespace content

#endif  //! CONTENT_INPUT_TEXT_INPUT_H_
