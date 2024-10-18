// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONFIG_ENGINE_CONFIG_H_
#define CONTENT_CONFIG_ENGINE_CONFIG_H_

#include "base/math/vector.h"
#include "base/memory/ref_counted.h"
#include "components/filesystem/filesystem.h"
#include "content/common/content_utils.h"

#include <unordered_map>

namespace content {

class CoreConfigure : public base::RefCounted<CoreConfigure> {
 public:
  CoreConfigure() = default;

  CoreConfigure(const CoreConfigure&) = delete;
  CoreConfigure& operator=(const CoreConfigure&) = delete;

  void LoadCommandLine(int argc, char** argv);
  bool LoadConfigure(SDL_IOStream* filestream, const std::string& app);

  inline std::vector<std::string>& args() { return args_; }
  inline std::string& executable_file() { return executable_file_; }

  inline std::string& game_rtp() { return game_rtp_; }
  inline std::string& game_title() { return game_title_; }
  inline std::string& game_scripts() { return game_scripts_; }
  inline bool& game_debug() { return game_debug_; }
  inline bool& game_battle_test() { return game_battle_test_; }

  inline APIVersion& content_version() { return rgss_version_; }
  inline bool& disable_audio() { return disable_audio_; }
  inline bool& async_renderer() { return async_renderer_; }
  inline bool& disable_menu() { return disable_menu_; }
  inline bool& disable_reset() { return disable_reset_; }
  inline base::Vec2i& initial_resolution() { return initial_resolution_; }
  inline std::vector<std::string>& load_paths() { return load_paths_; }
  inline std::string& default_font_path() { return default_font_path_; }

 private:
  std::vector<std::string> args_;
  std::string executable_file_;

  std::string game_rtp_;
  std::string game_title_;
  std::string game_scripts_;
  bool game_debug_;
  bool game_battle_test_;

  APIVersion rgss_version_;
  bool disable_audio_;
  bool async_renderer_;
  bool disable_menu_;
  bool disable_reset_;
  base::Vec2i initial_resolution_;
  std::vector<std::string> load_paths_;
  std::string default_font_path_;
};

}  // namespace content

#endif  //! CONTENT_CONFIG_ENGINE_CONFIG_H_
