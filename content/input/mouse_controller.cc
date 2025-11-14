// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/mouse_controller.h"

#include "SDL3/SDL_events.h"

#include "content/canvas/canvas_impl.h"
#include "content/context/execution_context.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// MouseImpl Implement

MouseImpl::MouseImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context) {
  UpdateInternal();
}

MouseImpl::~MouseImpl() = default;

void MouseImpl::ProcessEvent(
    const std::optional<EventController::MouseEventData>& event) {
  if (event) {
    switch (event->type) {
      case MouseEvent::TYPE_MOUSE_BUTTON_DOWN:
      case MouseEvent::TYPE_MOUSE_BUTTON_UP:
        raw_state_.states[event->button_id] = event->button_is_down;
        raw_state_.clicks[event->button_id] = event->button_clicks;
      case MouseEvent::TYPE_MOUSE_MOTION:
        raw_state_.position = event->relative_position;
        break;
      case MouseEvent::TYPE_MOUSE_WHEEL:
        raw_state_.scroll = event->wheel_offset;
        break;
      default:
        break;
    }
  } else {
    for (auto& it : raw_state_.states)
      it = false;
  }
}

void MouseImpl::Update(ExceptionState& exception_state) {
  UpdateInternal();
}

float MouseImpl::GetX(ExceptionState& exception_state) {
  return raw_state_.position.x;
}

float MouseImpl::GetY(ExceptionState& exception_state) {
  return raw_state_.position.y;
}

void MouseImpl::SetPosition(float x, float y, ExceptionState& exception_state) {
  SDL_WarpMouseInWindow(context()->window->AsSDLWindow(), x, y);
}

bool MouseImpl::IsDown(int32_t button, ExceptionState& exception_state) {
  return states_[button].down;
}

bool MouseImpl::IsUp(int32_t button, ExceptionState& exception_state) {
  return states_[button].up;
}

bool MouseImpl::IsDouble(int32_t button, ExceptionState& exception_state) {
  return states_[button].click_count == 2;
}

bool MouseImpl::IsPressed(int32_t button, ExceptionState& exception_state) {
  return states_[button].pressed;
}

bool MouseImpl::IsMoved(ExceptionState& exception_state) {
  return entity_state_.is_moved;
}

float MouseImpl::GetScrollX(ExceptionState& exception_state) {
  return entity_state_.scroll.x;
}

float MouseImpl::GetScrollY(ExceptionState& exception_state) {
  return entity_state_.scroll.y;
}

void MouseImpl::SetCursor(scoped_refptr<Bitmap> cursor,
                          int32_t hot_x,
                          int32_t hot_y,
                          ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> bitmap = CanvasImpl::FromBitmap(cursor);
  if (Disposable::IsValid(bitmap.get())) {
    SDL_Cursor* cur =
        SDL_CreateColorCursor(bitmap->RequireMemorySurface(), hot_x, hot_y);
    SDL_SetCursor(cur);
  } else {
    SDL_Cursor* cur_old = SDL_GetCursor();
    SDL_Cursor* cur = SDL_GetDefaultCursor();
    SDL_SetCursor(cur);
    SDL_DestroyCursor(cur_old);
  }
}

bool MouseImpl::Capture(bool enable, ExceptionState& exception_state) {
  return SDL_CaptureMouse(enable);
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Visible,
    bool,
    MouseImpl,
    { return SDL_CursorVisible(); },
    { value ? SDL_ShowCursor() : SDL_HideCursor(); });

void MouseImpl::UpdateInternal() {
  for (size_t i = 0; i < MOUSE_BUTTON_COUNT; ++i) {
    const int32_t press_state = raw_state_.states[i];
    states_[i].click_count = raw_state_.clicks[i];
    states_[i].down = !states_[i].pressed && press_state;
    states_[i].up = states_[i].pressed && !press_state;
    states_[i].pressed = press_state;
  }

  entity_state_.is_moved = entity_state_.last_position != raw_state_.position;
  entity_state_.last_position = raw_state_.position;

  entity_state_.scroll.x = 0.0f;
  if (entity_state_.last_scroll.x != raw_state_.scroll.x)
    entity_state_.scroll.x = raw_state_.scroll.x - entity_state_.last_scroll.x;
  entity_state_.last_scroll.x = raw_state_.scroll.x;

  entity_state_.scroll.y = 0.0f;
  if (entity_state_.last_scroll.y != raw_state_.scroll.y)
    entity_state_.scroll.y = raw_state_.scroll.y - entity_state_.last_scroll.y;
  entity_state_.last_scroll.y = raw_state_.scroll.y;
}

}  // namespace content
