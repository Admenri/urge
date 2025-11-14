// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_INPUT_MOUSE_CONTROLLER_H_
#define CONTENT_INPUT_MOUSE_CONTROLLER_H_

#include <array>

#include "content/context/engine_object.h"
#include "content/profile/content_profile.h"
#include "content/public/engine_mouse.h"
#include "content/worker/event_controller.h"
#include "ui/widget/widget.h"

namespace content {

class MouseImpl : public Mouse, public EngineObject {
 public:
  enum Button {
    Left = SDL_BUTTON_LEFT,
    Middle = SDL_BUTTON_MIDDLE,
    Right = SDL_BUTTON_RIGHT,
    X1 = SDL_BUTTON_X1,
    X2 = SDL_BUTTON_X2,
  };

  MouseImpl(ExecutionContext* execution_context);
  ~MouseImpl() override;

  MouseImpl(const MouseImpl&) = delete;
  MouseImpl& operator=(const MouseImpl&) = delete;

  void ProcessEvent(
      const std::optional<EventController::MouseEventData>& event);

 protected:
  void Update(ExceptionState& exception_state) override;
  float GetX(ExceptionState& exception_state) override;
  float GetY(ExceptionState& exception_state) override;
  void SetPosition(float x, float y, ExceptionState& exception_state) override;
  bool IsDown(int32_t button, ExceptionState& exception_state) override;
  bool IsUp(int32_t button, ExceptionState& exception_state) override;
  bool IsDouble(int32_t button, ExceptionState& exception_state) override;
  bool IsPressed(int32_t button, ExceptionState& exception_state) override;
  bool IsMoved(ExceptionState& exception_state) override;
  float GetScrollX(ExceptionState& exception_state) override;
  float GetScrollY(ExceptionState& exception_state) override;
  void SetCursor(scoped_refptr<Bitmap> cursor,
                 int32_t hot_x,
                 int32_t hot_y,
                 ExceptionState& exception_state) override;
  bool Capture(bool enable, ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);

 private:
  void UpdateInternal();

  struct MouseState {
    base::Vec2 position;
    base::Vec2 scroll;
    std::array<bool, MOUSE_BUTTON_COUNT> states;
    std::array<uint8_t, MOUSE_BUTTON_COUNT> clicks;
  };

  struct BindingState {
    int32_t up = 0;
    int32_t down = 0;
    int32_t pressed = 0;
    int32_t click_count = 0;
  };

  struct {
    base::Vec2 last_position;
    base::Vec2 scroll;
    base::Vec2 last_scroll;
    int32_t is_moved = 0;
  } entity_state_;

  MouseState raw_state_;
  std::array<BindingState, MOUSE_BUTTON_COUNT> states_;
};

}  // namespace content

#endif  //! CONTENT_INPUT_MOUSE_CONTROLLER_H_
