// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/input/mouse_controller.h"

#include "content/canvas/canvas_impl.h"

namespace content {

MouseImpl::MouseImpl(base::WeakPtr<ui::Widget> window, ContentProfile* profile)
    : window_(window), profile_(profile), enable_update_(true) {}

MouseImpl::~MouseImpl() = default;

void MouseImpl::Update(ExceptionState& exception_state) {
  if (!enable_update_)
    return;

  auto& mouse_state = window_->GetMouseState();
  for (size_t i = 0; i < states_.size(); ++i) {
    bool press_state = mouse_state.states[i];

    states_[i].down = !states_[i].pressed && press_state;
    states_[i].up = states_[i].pressed && !press_state;

    states_[i].pressed = press_state;
    states_[i].click_count = mouse_state.clicks[i];
    mouse_state.clicks[i] = 0;
  }

  if (entity_state_.last_x != mouse_state.x ||
      entity_state_.last_y != mouse_state.y)
    entity_state_.moved = true;
  else
    entity_state_.moved = false;
  entity_state_.last_x = mouse_state.x;
  entity_state_.last_y = mouse_state.y;

  entity_state_.scroll_x = 0;
  if (entity_state_.last_scroll_x != mouse_state.scroll_x)
    entity_state_.scroll_x = mouse_state.scroll_x - entity_state_.last_scroll_x;
  entity_state_.last_scroll_x = mouse_state.scroll_x;

  entity_state_.scroll_y = 0;
  if (entity_state_.last_scroll_y != mouse_state.scroll_y)
    entity_state_.scroll_y = mouse_state.scroll_y - entity_state_.last_scroll_y;
  entity_state_.last_scroll_y = mouse_state.scroll_y;
}

int32_t MouseImpl::GetX(ExceptionState& exception_state) {
  return window_->GetMouseState().x;
}

int32_t MouseImpl::GetY(ExceptionState& exception_state) {
  return window_->GetMouseState().y;
}

void MouseImpl::SetPosition(int32_t x,
                            int32_t y,
                            ExceptionState& exception_state) {
  base::Vec2 origin(x, y);
  auto& mouse_state = window_->GetMouseState();
  base::Vec2 scale = mouse_state.resolution / mouse_state.screen;
  base::Vec2 pos = base::Vec2(mouse_state.screen_offset) + origin / scale;
  SDL_WarpMouseInWindow(window_->AsSDLWindow(), pos.x, pos.y);
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
  return entity_state_.moved;
}

int32_t MouseImpl::GetScrollX(ExceptionState& exception_state) {
  return entity_state_.scroll_x;
}

int32_t MouseImpl::GetScrollY(ExceptionState& exception_state) {
  return entity_state_.scroll_y;
}

void MouseImpl::SetCursor(scoped_refptr<Bitmap> cursor,
                          int32_t hot_x,
                          int32_t hot_y,
                          ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> bitmap = CanvasImpl::FromBitmap(cursor);
  if (bitmap && bitmap->GetAgent()) {
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

bool MouseImpl::Get_Visible(ExceptionState& exception_state) {
  return window_->GetMouseState().visible;
}

void MouseImpl::Put_Visible(const bool& value,
                            ExceptionState& exception_state) {
  window_->GetMouseState().visible = value;
}

}  // namespace content
