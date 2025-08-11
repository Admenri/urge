// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/misc/misc_system.h"

#include <iostream>

#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_misc.h"
#include "SDL3/SDL_platform.h"

#include "components/version/version.h"

namespace content {

MiscSystem::MiscSystem(ContentProfile* profile,
                       base::WeakPtr<ui::Widget> window,
                       filesystem::IOService* io_service)
    : profile_(profile), window_(window), io_service_(io_service) {}

MiscSystem::~MiscSystem() = default;

std::string MiscSystem::GetBuildDate(ExceptionState& exception_state) {
  return URGE_BUILD_DATE;
}

std::string MiscSystem::GetRevision(ExceptionState& exception_state) {
  return URGE_GIT_REVISION;
}

std::string MiscSystem::GetPlatform(ExceptionState& exception_state) {
  return SDL_GetPlatform();
}

int32_t MiscSystem::GetAPIVersion(ExceptionState& exception_state) {
  return static_cast<int32_t>(profile_->api_version);
}

void MiscSystem::OpenURL(const std::string& path,
                         ExceptionState& exception_state) {
  if (!SDL_OpenURL(path.c_str()))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "OpenURL: %s",
                               SDL_GetError());
}

std::string MiscSystem::Gets(ExceptionState& exception_state) {
  std::string in(1 << 10, 0);
  std::fgets(in.data(), in.size(), stdin);
  return in.c_str();
}

void MiscSystem::Puts(const std::string& message,
                      ExceptionState& exception_state) {
  LOG(INFO) << message;
}

void MiscSystem::Alert(const std::string& message,
                       ExceptionState& exception_state) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           window_->GetTitle().c_str(), message.c_str(),
                           window_->AsSDLWindow());
}

bool MiscSystem::Confirm(const std::string& message,
                         ExceptionState& exception_state) {
  std::string title = window_->GetTitle();

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

bool MiscSystem::AddLoadPath(const std::string& new_path,
                             const std::string& mount_point,
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

bool MiscSystem::RemoveLoadPath(const std::string& old_path,
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

bool MiscSystem::IsFileExisted(const std::string& filepath,
                               ExceptionState& exception_state) {
  return io_service_->Exists(filepath);
}

std::vector<std::string> MiscSystem::EnumDirectory(
    const std::string& dirpath,
    ExceptionState& exception_state) {
  return io_service_->EnumDir(dirpath);
}

}  // namespace content
