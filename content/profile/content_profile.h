// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PROFILE_CONTENT_PROFILE_H_
#define CONTENT_PROFILE_CONTENT_PROFILE_H_

#include <string>

#include "SDL3/SDL_iostream.h"

#include "base/math/vector.h"
#include "base/memory/allocator.h"

namespace content {

class ContentProfile {
 public:
  enum class APIVersion {
    UNKNOWN = 0,
    RGSS1,
    RGSS2,
    RGSS3,
  };

  ContentProfile(const base::String& app, SDL_IOStream* stream);
  ~ContentProfile();

  ContentProfile(const ContentProfile&) = delete;
  ContentProfile& operator=(const ContentProfile&) = delete;

  void LoadCommandLine(int32_t argc, char** argv);
  bool LoadConfigure(const base::String& app);

  // App
  base::Vector<base::String> args;
  base::String program_name;

  // Game
  base::String window_title = "Engine Widget";
  base::String script_path = "Data/Scripts.rxdata";

  // Commandline
  bool game_debug = false;
  bool game_battle_test = false;

  // Engine
  APIVersion api_version = APIVersion::UNKNOWN;
  base::String default_font_path = "Fonts/Default.ttf";
  base::String i18n_xml_path;
  base::Vec2i window_size;
  base::Vec2i resolution;

  // GUI
  bool disable_settings = false;
  bool disable_fps_monitor = false;
  bool disable_reset = false;

  // Renderer
  base::String driver_backend = "UNDEFINED";
  bool render_validation =
#if DILIGENT_DEVELOPMENT
      true;
#else
      false;
#endif
  int32_t frame_rate = 60;
  uint32_t vsync = 1;
  bool keep_ratio = true;
  bool fullscreen = false;
  bool smooth_scale = false;
  bool allow_skip_frame = true;
  bool background_running = true;

  // Platform
  bool disable_ime = false;
  base::String orientation = "LandscapeLeft LandscapeRight";

  // Features
  bool disable_audio = false;
  bool sprite_vertical_sort = true;

 private:
  SDL_IOStream* ini_stream_;
};

}  // namespace content

#endif  //! CONTENT_PROFILE_CONTENT_PROFILE_H_
