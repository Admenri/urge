// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/key_event.h"

#include "content/context/execution_context.h"

namespace content {

// static
std::vector<scoped_refptr<KeyEvent>> KeyEvent::Update(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  std::vector<EventController::KeyEventData> raw_events =
      execution_context->event_controller->key_events();

  std::vector<scoped_refptr<KeyEvent>> filtered_events;
  for (auto& it : raw_events)
    filtered_events.push_back(base::MakeRefCounted<KeyEventImpl>(it));

  return filtered_events;
}

///////////////////////////////////////////////////////////////////////////////
// KeyEventImpl Implement

KeyEventImpl::KeyEventImpl(EventController::KeyEventData event)
    : event_(event) {}

KeyEventImpl::~KeyEventImpl() = default;

KeyEvent::Type KeyEventImpl::GetType(ExceptionState& exception_state) {
  return event_.type;
}

int32_t KeyEventImpl::GetDeviceID(ExceptionState& exception_state) {
  return event_.keyboard_id;
}

int32_t KeyEventImpl::GetScancode(ExceptionState& exception_state) {
  return event_.scancode;
}

int32_t KeyEventImpl::GetKey(ExceptionState& exception_state) {
  return event_.keycode;
}

int32_t KeyEventImpl::GetModifier(ExceptionState& exception_state) {
  return event_.modifier;
}

bool KeyEventImpl::GetDown(ExceptionState& exception_state) {
  return event_.is_down;
}

bool KeyEventImpl::GetRepeat(ExceptionState& exception_state) {
  return event_.is_repeat;
}

}  // namespace content
