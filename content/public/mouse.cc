// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/mouse.h"

namespace content {

Mouse::Mouse(base::WeakPtr<ui::Widget> window) : window_(window) {}

void Mouse::Update() {
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

int Mouse::GetX() {
  return window_->GetMouseState().x;
}

int Mouse::GetY() {
  return window_->GetMouseState().y;
}

void Mouse::SetPos(int x, int y) {
  base::Vec2 origin(x, y);
  auto& mouse_state = window_->GetMouseState();
  base::Vec2 scale = mouse_state.resolution / mouse_state.screen;
  base::Vec2 pos = base::Vec2(mouse_state.screen_offset) + origin / scale;
  SDL_WarpMouseInWindow(window_->AsSDLWindow(), pos.x, pos.y);
}

bool Mouse::GetVisible() {
  return window_->GetMouseState().visible;
}

void Mouse::SetVisible(bool visible) {
  window_->GetMouseState().visible = visible;
}

bool Mouse::IsDown(int button) {
  return states_[button].down;
}

bool Mouse::IsUp(int button) {
  return states_[button].up;
}

bool Mouse::IsDoubleClick(int button) {
  return states_[button].click_count == 2;
}

bool Mouse::IsPressed(int button) {
  return states_[button].pressed;
}

bool Mouse::IsMoved() {
  return entity_state_.moved;
}

int Mouse::GetScrollX() {
  return entity_state_.scroll_x;
}

int Mouse::GetScrollY() {
  return entity_state_.scroll_y;
}

void Mouse::SetCursor(scoped_refptr<Bitmap> cursor, int hot_x, int hot_y) {
  SDL_Cursor* cur =
      SDL_CreateColorCursor(cursor->SurfaceRequired(), hot_x, hot_y);
  SDL_SetCursor(cur);
}

void Mouse::ClearCursor() {
  SDL_Cursor* curOld = SDL_GetCursor();
  SDL_Cursor* cur = SDL_GetDefaultCursor();
  SDL_SetCursor(cur);
  SDL_DestroyCursor(curOld);
}

}  // namespace content
