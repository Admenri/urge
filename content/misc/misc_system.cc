// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/misc/misc_system.h"

#include <iostream>

#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_misc.h"
#include "SDL3/SDL_platform.h"

namespace content {

MiscSystem::MiscSystem(base::WeakPtr<ui::Widget> window,
                       filesystem::IOService* io_service)
    : window_(window), io_service_(io_service) {}

MiscSystem::~MiscSystem() = default;

base::String MiscSystem::GetPlatform(ExceptionState& exception_state) {
  return SDL_GetPlatform();
}

void MiscSystem::OpenURL(const base::String& path,
                         ExceptionState& exception_state) {
  if (!SDL_OpenURL(path.c_str()))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "OpenURL: %s",
                               SDL_GetError());
}

base::String MiscSystem::Gets(ExceptionState& exception_state) {
  base::String in;
  std::cin >> in;
  return in;
}

void MiscSystem::Puts(const base::String& message,
                      ExceptionState& exception_state) {
  LOG(INFO) << message;
}

void MiscSystem::Alert(const base::String& message,
                       ExceptionState& exception_state) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           window_->GetTitle().c_str(), message.c_str(),
                           window_->AsSDLWindow());
}

bool MiscSystem::Confirm(const base::String& message,
                         ExceptionState& exception_state) {
  base::String title = window_->GetTitle();

  SDL_MessageBoxData messagebox_data;
  messagebox_data.flags =
      SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT;
  messagebox_data.window = window_->AsSDLWindow();
  messagebox_data.title = title.c_str();
  messagebox_data.message = message.c_str();

  SDL_MessageBoxButtonData buttons[2];
  buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT |
                     SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
  buttons[0].buttonID = 0;
  buttons[0].text = "Cancel";
  buttons[1].flags = 0;
  buttons[1].buttonID = 1;
  buttons[1].text = "OK";

  messagebox_data.numbuttons = std::size(buttons);
  messagebox_data.buttons = buttons;

  int button_id = 0;
  SDL_ShowMessageBox(&messagebox_data, &button_id);

  return button_id;
}

bool MiscSystem::AddLoadPath(const base::String& new_path,
                             const base::String& mount_point,
                             bool append_to_path,
                             ExceptionState& exception_state) {
  auto result = io_service_->AddLoadPath(new_path.c_str(), mount_point.c_str(),
                                         append_to_path);
  if (!result) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to add path: %s",
                               io_service_->GetLastError().c_str());
    return false;
  }

  return !!result;
}

bool MiscSystem::RemoveLoadPath(const base::String& old_path,
                                ExceptionState& exception_state) {
  auto result = io_service_->RemoveLoadPath(old_path.c_str());
  if (!result) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to remove path: %s",
                               io_service_->GetLastError().c_str());
    return false;
  }

  return !!result;
}

bool MiscSystem::IsFileExisted(const base::String& filepath,
                               ExceptionState& exception_state) {
  return io_service_->Exists(filepath);
}

base::Vector<base::String> MiscSystem::EnumDirectory(
    const base::String& dirpath,
    ExceptionState& exception_state) {
  return io_service_->EnumDir(dirpath);
}

}  // namespace content
