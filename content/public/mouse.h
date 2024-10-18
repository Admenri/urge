// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_MOUSE_H_
#define CONTENT_PUBLIC_MOUSE_H_

#include "content/public/bitmap.h"
#include "ui/widget/widget.h"

#include "SDL3/SDL_mouse.h"

#include <array>

namespace content {

class Mouse : public base::RefCounted<Mouse> {
 public:
  enum Button {
    Left = SDL_BUTTON_LEFT,
    Middle = SDL_BUTTON_MIDDLE,
    Right = SDL_BUTTON_RIGHT,
    X1 = SDL_BUTTON_X1,
    X2 = SDL_BUTTON_X2,
  };

  Mouse(base::WeakPtr<ui::Widget> window);

  Mouse(const Mouse&) = delete;
  Mouse& operator=(const Mouse&) = delete;

  void Update();

  int GetX();
  int GetY();
  void SetPos(int x, int y);
  bool IsDown(int button);
  bool IsUp(int button);
  bool IsDoubleClick(int button);
  bool IsPressed(int button);
  bool IsMoved();
  int GetScrollX();
  int GetScrollY();
  void SetCursor(scoped_refptr<Bitmap> cursor, int hot_x, int hot_y);
  void ClearCursor();

  bool GetVisible();
  void SetVisible(bool visible);

 private:
  struct BindingState {
    bool up = false;
    bool down = false;
    int click_count = 0;
    bool pressed = false;
  };

  struct MouseState {
    float last_x = 0, last_y = 0;
    bool moved = false;
    int scroll_x = 0, scroll_y = 0;
    int last_scroll_x = 0, last_scroll_y = 0;
  } entity_state_;

  std::array<BindingState, sizeof(ui::Widget::MouseState::states)> states_;
  base::WeakPtr<ui::Widget> window_;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_MOUSE_H_
