// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "SDL3/SDL_iostream.h"
#include "glm/vec2.hpp"

#include "content/common/object.h"

namespace content {

struct CoreProfile : public Singleton<CoreProfile> {
  CoreProfile(SDL_IOStream* config_file);

  CoreProfile(const CoreProfile&) = delete;
  CoreProfile& operator=(const CoreProfile&) = delete;

  struct {
    uint32_t api_version = 0;
    std::string scripts = "Data/Scripts.rxdata";
  } core;

  struct {
    std::string title = "Untitled";
    glm::ivec2 size = glm::ivec2(640, 480);
    bool resizable = false;
  } window;

  struct {
    std::string backend = "default";
    bool validation = false;
  } render;

  struct {
    std::string default_path = "Fonts/Default.ttf";
  } font;
};

}  // namespace content
