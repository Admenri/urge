// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_EVENT_CONTROLLER_H_
#define CONTENT_WORKER_EVENT_CONTROLLER_H_

#include <array>
#include <vector>

#include "SDL3/SDL_events.h"

#include "base/math/rectangle.h"
#include "content/public/engine_keyevent.h"
#include "content/public/engine_mouseevent.h"
#include "content/public/engine_textinputevent.h"
#include "content/public/engine_touchevent.h"

namespace content {

#define MOUSE_BUTTON_COUNT 5

class EventController {
 public:
  // Custom event handler data
  struct CommonData {
    uint64_t timestamp;
  };

  // Keyboard event data
  struct KeyEventData : public CommonData {
    KeyEvent::Type type;
    SDL_KeyboardID keyboard_id;
    SDL_Scancode scancode;
    SDL_Keycode keycode;
    SDL_Keymod modifier;
    int32_t is_down;
    int32_t is_repeat;
  };

  // Mouse event data
  struct MouseEventData : public CommonData {
    MouseEvent::Type type;
    SDL_MouseID mouse_id;
    base::Vec2 relative_position;
    int32_t button_id;
    int32_t button_is_down;
    int32_t button_clicks;
    int32_t motion_state;
    base::Vec2 relative_offset;
    MouseEvent::WheelState wheel_dir;
    base::Vec2 wheel_offset;
  };

  // Touch event data
  struct TouchEventData : public CommonData {
    TouchEvent::Type type;
    SDL_TouchID touch_id;
    SDL_FingerID finger;
    base::Vec2 relative_position;
    base::Vec2 delta_offset;
    float pressure;
  };

  // IME input event data
  struct TextInputEventData : public CommonData {
    TextInputEvent::Type type;
    std::string text;
    int32_t select_start;
    int32_t select_length;
  };

  EventController();
  ~EventController();

  EventController(const EventController&) = delete;
  EventController& operator=(const EventController&) = delete;

  // Process game window
  void ClearPendingEvents();
  void DispatchEvent(const SDL_Event* event,
                     const base::Vec2i& window_size,
                     const base::Vec2i& screen_size,
                     const base::Rect& bound_in_screen);

  // For event reference (readonly)
  const std::vector<KeyEventData>& key_events() const { return key_events_; }
  const std::vector<MouseEventData>& mouse_events() const {
    return mouse_events_;
  }
  const std::vector<TouchEventData>& touch_events() const {
    return touch_events_;
  }
  const std::vector<TextInputEventData>& text_input_events() const {
    return text_input_events_;
  }

 private:
  std::vector<KeyEventData> key_events_;
  std::vector<MouseEventData> mouse_events_;
  std::vector<TouchEventData> touch_events_;
  std::vector<TextInputEventData> text_input_events_;
};

}  // namespace content

#endif  //! CONTENT_WORKER_EVENT_CONTROLLER_H_
