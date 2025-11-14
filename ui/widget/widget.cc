// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/widget/widget.h"

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"

#include <mutex>

namespace ui {

const char kNativeWidgetKey[] = "UIBase::Widget";

Widget* Widget::FromWindowID(SDL_WindowID window_id) {
  SDL_Window* sdl_window = SDL_GetWindowFromID(window_id);
  return static_cast<Widget*>(SDL_GetPointerProperty(
      SDL_GetWindowProperties(sdl_window), kNativeWidgetKey, nullptr));
}

Widget::Widget() : window_(nullptr), window_id_(SDL_WindowID()) {
  SDL_AddEventWatch(&Widget::UIEventDispatcher, this);
}

Widget::~Widget() {
  SDL_RemoveEventWatch(&Widget::UIEventDispatcher, this);

  if (window_)
    SDL_DestroyWindow(window_);
}

void Widget::Init(InitParams params) {
  auto property_id = SDL_CreateProperties();

  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN,
                         params.fullscreen);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN,
                         params.activitable);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN,
                         params.resizable);

  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN,
                         true);
  if (params.window_state == WindowPlacement::Maximum)
    SDL_SetBooleanProperty(property_id,
                           SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, true);
  else if (params.window_state == WindowPlacement::Minimum)
    SDL_SetBooleanProperty(property_id,
                           SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, true);

  SDL_SetStringProperty(property_id, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                        params.title.c_str());

  if (params.position) {
    SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_X_NUMBER,
                          params.position->x);
    SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_Y_NUMBER,
                          params.position->y);
  } else {
    SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_X_NUMBER,
                          SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_Y_NUMBER,
                          SDL_WINDOWPOS_CENTERED);
  }

  SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                        params.size.x);
  SDL_SetNumberProperty(property_id, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                        params.size.y);
  SDL_SetBooleanProperty(property_id,
                         SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN,
                         params.transparent);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_MENU_BOOLEAN,
                         params.menu_window);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_TOOLTIP_BOOLEAN,
                         params.tooltip_window);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_UTILITY_BOOLEAN,
                         params.utility_window);
  if (params.parent_window)
    SDL_GetPointerProperty(property_id, SDL_PROP_WINDOW_CREATE_PARENT_POINTER,
                           params.parent_window->AsSDLWindow());

  SDL_SetBooleanProperty(property_id,
                         SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN,
                         params.hpixeldensity);
  SDL_SetBooleanProperty(property_id, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN,
                         params.borderless);
  SDL_SetBooleanProperty(property_id,
                         SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN,
                         params.always_on_top);

  // No internal graphics context manager
  SDL_SetBooleanProperty(
      property_id, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN,
      true);

  window_ = SDL_CreateWindowWithProperties(property_id);
  if (!window_)
    LOG(INFO) << "[UI] " << SDL_GetError();

  if (params.dpi_awareness) {
    const float dpi = SDL_GetWindowDisplayScale(window_);
    SDL_SetWindowSize(window_, params.size.x * dpi, params.size.y * dpi);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
  }

  auto window_prop = SDL_GetWindowProperties(window_);
  SDL_SetPointerProperty(window_prop, kNativeWidgetKey, this);
  SDL_DestroyProperties(property_id);

  if (params.window_state == WindowPlacement::Show)
    SDL_ShowWindow(window_);
  else if (params.window_state == WindowPlacement::Hide)
    SDL_HideWindow(window_);

  window_id_ = SDL_GetWindowID(window_);
}

void Widget::SetFullscreen(bool fullscreen) {
  SDL_SetWindowFullscreen(window_, fullscreen);
}

void Widget::SetTitle(const std::string& window_title) {
  SDL_SetWindowTitle(window_, window_title.c_str());
}

std::string Widget::GetTitle() const {
  return std::string(SDL_GetWindowTitle(window_));
}

bool Widget::IsFullscreen() const {
  return SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN;
}

base::Vec2i Widget::GetPosition() {
  base::Vec2i pos(0);
  SDL_GetWindowPosition(window_, &pos.x, &pos.y);
  return pos;
}

base::Vec2i Widget::GetSize() {
  base::Vec2i size(0);
  SDL_GetWindowSize(window_, &size.x, &size.y);
  return size;
}

bool Widget::UIEventDispatcher(void* userdata, SDL_Event* event) {
  Widget* self = static_cast<Widget*>(userdata);

  if (event->type == SDL_EVENT_KEY_DOWN) {
    if (event->key.windowID == self->window_id_) {
      if (event->key.scancode == SDL_SCANCODE_RETURN &&
          (event->key.mod & SDL_KMOD_ALT)) {
        // Toggle fullscreen
        bool fullscreen_state = self->IsFullscreen();
        self->SetFullscreen(!fullscreen_state);

        return true;
      }
    }
  }

  return true;
}

}  // namespace ui
