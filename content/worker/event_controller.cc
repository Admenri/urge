// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/event_controller.h"

namespace content {

EventController::EventController() {}

EventController::~EventController() = default;

void EventController::ClearPendingEvents() {
  key_events_.clear();
  mouse_events_.clear();
  touch_events_.clear();
  text_input_events_.clear();
}

void EventController::DispatchEvent(const SDL_Event* event,
                                    const base::Vec2i& window_size,
                                    const base::Vec2i& screen_size,
                                    const base::Rect& bound_in_screen) {
  // Global window position to game screen
  auto window_to_screen = [&](base::Vec2* out, base::Vec2 in) {
    out->x = static_cast<float>(in.x - bound_in_screen.x) /
             bound_in_screen.width * static_cast<float>(screen_size.x);
    out->y = static_cast<float>(in.y - bound_in_screen.y) /
             bound_in_screen.height * static_cast<float>(screen_size.y);
  };

  // Dispatch event with filter
  switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      auto& raw_event = event->key;
      KeyEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_KEY_DOWN);
      out_event.keyboard_id = raw_event.which;
      out_event.scancode = raw_event.scancode;
      out_event.keycode = raw_event.key;
      out_event.modifier = raw_event.mod;
      out_event.is_down = raw_event.down;
      out_event.is_repeat = raw_event.repeat;
      key_events_.push_back(out_event);
    } break;
    case SDL_EVENT_MOUSE_MOTION: {
      auto& raw_event = event->motion;
      MouseEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_MOUSE_MOTION);
      out_event.mouse_id = raw_event.which;
      window_to_screen(&out_event.relative_position,
                       base::Vec2(raw_event.x, raw_event.y));
      out_event.motion_state = raw_event.state;
      out_event.relative_offset.x =
          static_cast<float>(raw_event.xrel / window_size.x) * screen_size.x;
      out_event.relative_offset.y =
          static_cast<float>(raw_event.yrel / window_size.y) * screen_size.y;
      mouse_events_.push_back(out_event);
    } break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      auto& raw_event = event->button;
      MouseEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_MOUSE_MOTION);
      out_event.mouse_id = raw_event.which;
      window_to_screen(&out_event.relative_position,
                       base::Vec2(raw_event.x, raw_event.y));
      out_event.button_id = raw_event.button;
      out_event.button_is_down = raw_event.down;
      out_event.button_clicks = raw_event.clicks;
      mouse_events_.push_back(out_event);
    } break;
    case SDL_EVENT_MOUSE_WHEEL: {
      auto& raw_event = event->wheel;
      MouseEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_MOUSE_MOTION);
      out_event.mouse_id = raw_event.which;
      window_to_screen(&out_event.relative_position,
                       base::Vec2(raw_event.x, raw_event.y));
      out_event.wheel_dir =
          static_cast<MouseEvent::WheelState>(raw_event.direction);
      out_event.wheel_offset.x = raw_event.x;
      out_event.wheel_offset.y = raw_event.y;
      mouse_events_.push_back(out_event);
    } break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED: {
      auto& raw_event = event->tfinger;
      TouchEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_FINGER_DOWN);
      out_event.touch_id = raw_event.touchID;
      out_event.finger = raw_event.fingerID;
      window_to_screen(
          &out_event.relative_position,
          base::Vec2(raw_event.x * window_size.x, raw_event.y * window_size.y));
      out_event.delta_offset.x = raw_event.dx * window_size.x;
      out_event.delta_offset.y = raw_event.dy * window_size.y;
      out_event.pressure = raw_event.pressure;
      touch_events_.push_back(out_event);
    } break;
    case SDL_EVENT_TEXT_EDITING: {
      auto& raw_event = event->edit;
      TextInputEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_TEXT_EDITING);
      out_event.text = raw_event.text;
      out_event.select_start = raw_event.start;
      out_event.select_length = raw_event.length;
      text_input_events_.push_back(out_event);
    } break;
    case SDL_EVENT_TEXT_INPUT: {
      auto& raw_event = event->text;
      TextInputEventData out_event{};
      out_event.timestamp = raw_event.timestamp;
      out_event.type = static_cast<decltype(out_event.type)>(
          event->type - SDL_EVENT_TEXT_EDITING);
      out_event.text = raw_event.text;
      out_event.select_start = -1;
      out_event.select_length = -1;
      text_input_events_.push_back(out_event);
    } break;
    default:
      break;
  }
}

}  // namespace content
