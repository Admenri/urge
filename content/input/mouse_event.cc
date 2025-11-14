// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/mouse_event.h"

#include "content/context/execution_context.h"

namespace content {

// static
std::vector<scoped_refptr<MouseEvent>> MouseEvent::Update(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  std::vector<EventController::MouseEventData> raw_events =
      execution_context->event_controller->mouse_events();

  std::vector<scoped_refptr<MouseEvent>> filtered_events;
  for (auto& it : raw_events)
    filtered_events.push_back(base::MakeRefCounted<MouseEventImpl>(it));

  return filtered_events;
}

///////////////////////////////////////////////////////////////////////////////
// MouseEventImpl Implement

MouseEventImpl::MouseEventImpl(EventController::MouseEventData event)
    : event_(event) {}

MouseEventImpl::~MouseEventImpl() = default;

MouseEvent::Type MouseEventImpl::GetType(ExceptionState& exception_state) {
  return event_.type;
}

int32_t MouseEventImpl::GetMouseID(ExceptionState& exception_state) {
  return event_.mouse_id;
}

float MouseEventImpl::GetX(ExceptionState& exception_state) {
  return event_.relative_position.x;
}

float MouseEventImpl::GetY(ExceptionState& exception_state) {
  return event_.relative_position.y;
}

int32_t MouseEventImpl::GetButton(ExceptionState& exception_state) {
  return event_.button_id;
}

bool MouseEventImpl::GetButtonDown(ExceptionState& exception_state) {
  return event_.button_is_down;
}

int32_t MouseEventImpl::GetButtonClicks(ExceptionState& exception_state) {
  return event_.button_clicks;
}

int32_t MouseEventImpl::GetMotion(ExceptionState& exception_state) {
  return event_.motion_state;
}

float MouseEventImpl::GetMotionX(ExceptionState& exception_state) {
  return event_.relative_offset.x;
}

float MouseEventImpl::GetMotionY(ExceptionState& exception_state) {
  return event_.relative_offset.y;
}

MouseEvent::WheelState MouseEventImpl::GetWheel(
    ExceptionState& exception_state) {
  return event_.wheel_dir;
}

float MouseEventImpl::GetWheelX(ExceptionState& exception_state) {
  return event_.wheel_offset.x;
}

float MouseEventImpl::GetWheelY(ExceptionState& exception_state) {
  return event_.wheel_offset.y;
}

}  // namespace content
