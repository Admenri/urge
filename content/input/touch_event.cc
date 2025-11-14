// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/touch_event.h"

#include "content/context/execution_context.h"

namespace content {

// static
std::vector<scoped_refptr<TouchEvent>> TouchEvent::Update(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  std::vector<EventController::TouchEventData> raw_events =
      execution_context->event_controller->touch_events();

  std::vector<scoped_refptr<TouchEvent>> filtered_events;
  for (auto& it : raw_events)
    filtered_events.push_back(base::MakeRefCounted<TouchEventImpl>(it));

  return filtered_events;
}

///////////////////////////////////////////////////////////////////////////////
// TouchEventImpl Implement

TouchEventImpl::TouchEventImpl(EventController::TouchEventData event)
    : event_(event) {}

TouchEventImpl::~TouchEventImpl() = default;

TouchEvent::Type TouchEventImpl::GetType(ExceptionState& exception_state) {
  return event_.type;
}

int32_t TouchEventImpl::GetDeviceID(ExceptionState& exception_state) {
  return event_.touch_id;
}

int32_t TouchEventImpl::GetFinger(ExceptionState& exception_state) {
  return event_.finger;
}

float TouchEventImpl::GetX(ExceptionState& exception_state) {
  return event_.relative_position.x;
}

float TouchEventImpl::GetY(ExceptionState& exception_state) {
  return event_.relative_position.y;
}

float TouchEventImpl::GetDeltaX(ExceptionState& exception_state) {
  return event_.delta_offset.x;
}

float TouchEventImpl::GetDeltaY(ExceptionState& exception_state) {
  return event_.delta_offset.y;
}

float TouchEventImpl::GetPressure(ExceptionState& exception_state) {
  return event_.pressure;
}

}  // namespace content
