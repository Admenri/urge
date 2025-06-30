// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_EVENT_CONTROLLER_H_
#define CONTENT_WORKER_EVENT_CONTROLLER_H_

#include <vector>

#include "SDL3/SDL_events.h"

#include "content/public/engine_keyevent.h"
#include "content/public/engine_mouseevent.h"
#include "content/public/engine_textinputevent.h"
#include "content/public/engine_touchevent.h"
#include "ui/widget/widget.h"

namespace content {

class EventController {
 public:
  using KeyEventData = struct {
    uint64_t timestamp;
    SDL_KeyboardID keyboard_id;

    KeyEvent::Type type;

    SDL_Scancode scancode;
    SDL_Keycode keycode;
    SDL_Keymod modifier;
    bool down;
    bool repeat;
  };

  using MouseEventData = struct {
    uint64_t timestamp;
    SDL_MouseID mouse_id;

    MouseEvent::Type type;

    int32_t x;
    int32_t y;

    int32_t button_id;
    int32_t button_down;
    int32_t button_clicks;

    int32_t motion_state;
    int32_t motion_relx;
    int32_t motion_rely;

    MouseEvent::WheelState wheel_dir;
    int32_t wheel_x;
    int32_t wheel_y;
  };

  using TouchEventData = struct {
    uint64_t timestamp;
    SDL_TouchID touch_id;

    TouchEvent::Type type;

    SDL_FingerID finger;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    float pressure;
  };

  using TextInputEventData = struct {
    uint64_t timestamp;

    TextInputEvent::Type type;

    base::String text;
    int32_t select_start;
    int32_t select_length;
  };

  EventController(base::WeakPtr<ui::Widget> window);
  ~EventController();

  EventController(const EventController&) = delete;
  EventController& operator=(const EventController&) = delete;

  base::WeakPtr<ui::Widget> GetHostWidget() const { return window_; }

  void DispatchEvent(SDL_Event* event);

  void PollKeyEvents(base::Vector<KeyEventData>& out) {
    out = key_events_;
    key_events_.clear();
  }

  void PollMouseEvents(base::Vector<MouseEventData>& out) {
    out = mouse_events_;
    mouse_events_.clear();
  }

  void PollTouchEvents(base::Vector<TouchEventData>& out) {
    out = touch_events_;
    touch_events_.clear();
  }

  void PollTextInputEvents(base::Vector<TextInputEventData>& out) {
    out = text_input_events_;
    text_input_events_.clear();
  }

 private:
  base::WeakPtr<ui::Widget> window_;
  base::Vector<KeyEventData> key_events_;
  base::Vector<MouseEventData> mouse_events_;
  base::Vector<TouchEventData> touch_events_;
  base::Vector<TextInputEventData> text_input_events_;
};

}  // namespace content

#endif  //! CONTENT_WORKER_EVENT_CONTROLLER_H_
