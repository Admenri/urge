// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/platform/engine_impl.h"

#include <iostream>

#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_misc.h"
#include "SDL3/SDL_platform.h"

#include "components/version/version.h"
#include "content/context/execution_context.h"

namespace content {

EngineImpl::EngineImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context) {}

EngineImpl::~EngineImpl() = default;

std::string EngineImpl::GetBuildDate(ExceptionState& exception_state) {
  return URGE_BUILD_DATE;
}

std::string EngineImpl::GetRevision(ExceptionState& exception_state) {
  return URGE_GIT_REVISION;
}

std::string EngineImpl::GetPlatform(ExceptionState& exception_state) {
  return SDL_GetPlatform();
}

int32_t EngineImpl::GetAPIVersion(ExceptionState& exception_state) {
  return static_cast<int32_t>(context()->engine_profile->api_version);
}

void EngineImpl::Reset(ExceptionState& exception_state) {
  for (auto it = disposable_elements_.tail(); it != disposable_elements_.end();
       it = it->previous()) {
    // Dispose all registered object.
    // Calling when f12 triggering.
    it->value()->Dispose();
  }
}

void EngineImpl::OpenURL(const std::string& path,
                         ExceptionState& exception_state) {
  if (!SDL_OpenURL(path.c_str()))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "OpenURL: %s",
                               SDL_GetError());
}

std::string EngineImpl::Gets(ExceptionState& exception_state) {
  std::string in(1 << 10, 0);
  std::fgets(in.data(), in.size(), stdin);
  return in.c_str();
}

void EngineImpl::Puts(const std::string& message,
                      ExceptionState& exception_state) {
  LOG(INFO) << message;
}

void EngineImpl::Alert(const std::string& message,
                       ExceptionState& exception_state) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           context()->window->GetTitle().c_str(),
                           message.c_str(), context()->window->AsSDLWindow());
}

bool EngineImpl::Confirm(const std::string& message,
                         ExceptionState& exception_state) {
  std::string title = context()->window->GetTitle();

  SDL_MessageBoxData messagebox_data;
  messagebox_data.flags =
      SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT;
  messagebox_data.window = context()->window->AsSDLWindow();
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

bool EngineImpl::AddLoadPath(const std::string& new_path,
                             const std::string& mount_point,
                             bool append_to_path,
                             ExceptionState& exception_state) {
  auto result = context()->io_service->AddLoadPath(
      new_path.c_str(), mount_point.c_str(), append_to_path);
  if (!result) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to add path: %s",
                               context()->io_service->GetLastError().c_str());
    return false;
  }

  return !!result;
}

bool EngineImpl::RemoveLoadPath(const std::string& old_path,
                                ExceptionState& exception_state) {
  auto result = context()->io_service->RemoveLoadPath(old_path.c_str());
  if (!result) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to remove path: %s",
                               context()->io_service->GetLastError().c_str());
    return false;
  }

  return !!result;
}

bool EngineImpl::IsFileExisted(const std::string& filepath,
                               ExceptionState& exception_state) {
  return context()->io_service->Exists(filepath);
}

std::vector<std::string> EngineImpl::EnumDirectory(
    const std::string& dirpath,
    ExceptionState& exception_state) {
  return context()->io_service->EnumDir(dirpath);
}

void EngineImpl::AddDisposable(Disposable* disp) {
  disposable_elements_.Append(disp);
}

}  // namespace content
