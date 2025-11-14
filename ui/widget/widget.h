// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WIDGET_WIDGET_H_
#define UI_WIDGET_WIDGET_H_

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_scancode.h"
#include "SDL3/SDL_touch.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "base/bind/callback_list.h"
#include "base/math/rectangle.h"
#include "base/memory/weak_ptr.h"

union SDL_Event;
struct SDL_Window;

namespace ui {

class Widget {
 public:
  enum class WindowPlacement {
    Show = 0,
    Hide,
    Maximum,
    Minimum,
  };

  struct InitParams {
    InitParams() = default;

    InitParams(InitParams&&) = default;
    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;

    std::string title;

    bool resizable = false;
    bool fullscreen =
#if defined(OS_ANDROID)
        true;
#else
        false;
#endif

    std::optional<base::Vec2i> position;
    base::Vec2i size;

    bool activitable = true;
    bool transparent = false;

    base::WeakPtr<Widget> parent_window = nullptr;
    bool tooltip_window = false;
    bool menu_window = false;
    bool utility_window = false;

    bool hpixeldensity = false;
    bool borderless = false;
    bool always_on_top = false;
    bool dpi_awareness = true;

    WindowPlacement window_state = WindowPlacement::Show;
  };

  Widget();
  virtual ~Widget();

  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;

  void Init(InitParams params);

  void SetFullscreen(bool fullscreen);
  bool IsFullscreen() const;

  void SetTitle(const std::string& window_title);
  std::string GetTitle() const;

  SDL_Window* AsSDLWindow() const { return window_; }
  base::WeakPtr<Widget> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  base::Vec2i GetPosition();
  base::Vec2i GetSize();

  static Widget* FromWindowID(SDL_WindowID window_id);
  SDL_WindowID GetWindowID() const { return window_id_; }

 private:
  static bool SDLCALL UIEventDispatcher(void* userdata, SDL_Event* event);

  SDL_Window* window_;
  SDL_WindowID window_id_;

  base::WeakPtrFactory<Widget> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_WIDGET_WIDGET_H_