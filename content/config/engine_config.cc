// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/config/engine_config.h"

#include "base/exception/exception.h"
#include "content/common/content_utils.h"
#include "inih/INIReader.h"
#include "rapidxml/rapidxml.hpp"

#include "SDL3/SDL_messagebox.h"

namespace content {

namespace {

void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

char* IniStreamReader(char* str, int num, void* stream) {
  SDL_IOStream* io = static_cast<SDL_IOStream*>(stream);

  memset(str, 0, num);
  char c;
  int i = 0;

  while (i < num - 1 && SDL_ReadIO(io, &c, 1)) {
    str[i++] = c;
    if (c == '\n')
      break;
  }

  str[i] = '\0';
  return i ? str : nullptr;
}

}  // namespace

void CoreConfigure::LoadCommandLine(int argc, char** argv) {
  for (int i = 0; i < argc; ++i)
    args_.push_back(argv[i]);

  game_debug_ = false;
  game_battle_test_ = false;

  for (int i = 0; i < argc; i++) {
    if (std::string(argv[i]) == "test" || std::string(argv[i]) == "debug")
      game_debug_ = true;
    if (std::string(argv[i]) == "btest")
      game_battle_test_ = true;
  }

  if (game_debug_)
    Debug() << "[App] Running debug test.";
  if (game_battle_test_)
    Debug() << "[App] Running battle test.";
}

bool CoreConfigure::LoadConfigure(SDL_IOStream* filestream,
                                  const std::string& app) {
  /* Parse configure */
  if (!filestream) {
    std::string str = "Failed to load configure file.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RGU Kernel", str.c_str(),
                             nullptr);
    return false;
  }

  INIReader reader(filestream, IniStreamReader);
  SDL_CloseIO(filestream);
  if (reader.ParseError()) {
    std::string str = "Error when parse configure.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RGU Kernel", str.c_str(),
                             nullptr);
    return false;
  }

  /* RGSS config part */
  game_rtp_ = reader.Get("Game", "RTP", std::string());
  game_title_ = reader.Get("Game", "Title", "RGU Default Widget");
  game_scripts_ = reader.Get("Game", "Scripts", std::string());
  ReplaceStringWidth(game_scripts_, '\\', '/');

  /* Core config */
  disable_menu_ = reader.GetBoolean("Kernel", "DisableMenu", false);
  disable_reset_ = reader.GetBoolean("Kernel", "DisableReset", false);
  async_renderer_ = reader.GetBoolean("Kernel", "AsyncRenderer", true);
  disable_audio_ = reader.GetBoolean("Kernel", "DisableAudio", false);
  rgss_version_ = (APIVersion)reader.GetInteger("Kernel", "RGSSVerison", 0);
  if (rgss_version_ == APIVersion::Null) {
    if (!game_scripts_.empty()) {
      rgss_version_ = APIVersion::RGSS1;

      const char* p = &game_scripts_[game_scripts_.size()];
      const char* head = &game_scripts_[0];

      while (--p != head)
        if (*p == '.')
          break;

      if (!strcmp(p, ".rvdata"))
        rgss_version_ = APIVersion::RGSS2;
      else if (!strcmp(p, ".rvdata2"))
        rgss_version_ = APIVersion::RGSS3;
    }
  }

  initial_resolution_ = base::Vec2i(
      reader.GetInteger("Game", "ScreenWidth",
                        rgss_version_ >= APIVersion::RGSS2 ? 544 : 640),
      reader.GetInteger("Game", "ScreenHeight",
                        rgss_version_ >= APIVersion::RGSS2 ? 416 : 480));

  int size = reader.GetInteger("Game", "LoadPathListSize", 0);
  for (int i = 0; i < size; ++i)
    load_paths_.push_back(reader.Get(
        "Game", "LoadPath" + std::to_string(i + 1), std::string()));

  default_font_path_ =
      reader.Get("Game", "DefaultFontPath", "Fonts/Default.ttf");

  return true;
}

}  // namespace content
