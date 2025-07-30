// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/profile/content_profile.h"

#include "base/buildflags/build.h"

#include "inih/INIReader.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {

namespace {

// https://stackoverflow.com/questions/28270310/how-to-easily-detect-utf8-encoding-in-the-string
bool CheckValidUTF8(const char* string) {
  if (!string)
    return true;

  const unsigned char* bytes = (const unsigned char*)string;
  unsigned int cp;
  int num;

  while (*bytes != 0x00) {
    if ((*bytes & 0x80) == 0x00) {
      // U+0000 to U+007F
      cp = (*bytes & 0x7F);
      num = 1;
    } else if ((*bytes & 0xE0) == 0xC0) {
      // U+0080 to U+07FF
      cp = (*bytes & 0x1F);
      num = 2;
    } else if ((*bytes & 0xF0) == 0xE0) {
      // U+0800 to U+FFFF
      cp = (*bytes & 0x0F);
      num = 3;
    } else if ((*bytes & 0xF8) == 0xF0) {
      // U+10000 to U+10FFFF
      cp = (*bytes & 0x07);
      num = 4;
    } else
      return false;

    bytes += 1;
    for (int i = 1; i < num; ++i) {
      if ((*bytes & 0xC0) != 0x80)
        return false;
      cp = (cp << 6) | (*bytes & 0x3F);
      bytes += 1;
    }

    if ((cp > 0x10FFFF) || ((cp >= 0xD800) && (cp <= 0xDFFF)) ||
        ((cp <= 0x007F) && (num != 1)) ||
        ((cp >= 0x0080) && (cp <= 0x07FF) && (num != 2)) ||
        ((cp >= 0x0800) && (cp <= 0xFFFF) && (num != 3)) ||
        ((cp >= 0x10000) && (cp <= 0x1FFFFF) && (num != 4)))
      return false;
  }

  return true;
}

void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

base::Vec2i GetVec2iFromString(std::string in,
                               const base::Vec2i& default_value) {
  size_t sep = in.find("|");
  if (sep != std::string::npos) {
    int32_t num1 = std::stoi(in.substr(0, sep).c_str());
    int32_t num2 = std::stoi(in.substr(sep + 1).c_str());
    return base::Vec2i(num1, num2);
  }

  return default_value;
}

char* IniStreamReader(char* str, int32_t num, void* stream) {
  SDL_IOStream* io = static_cast<SDL_IOStream*>(stream);

  memset(str, 0, num);
  char c;
  int32_t i = 0;

  while (i < num - 1 && SDL_ReadIO(io, &c, 1)) {
    str[i++] = c;
    if (c == '\n')
      break;
  }

  str[i] = '\0';
  return i ? str : nullptr;
}

#if defined(OS_WIN)
std::string ANSIFromUtf8(const std::string& asciiStr) {
  if (asciiStr.empty()) {
    return "";
  }

  // ASCII -> UTF-16
  int wcharsCount =
      MultiByteToWideChar(CP_ACP, 0, asciiStr.c_str(), -1, NULL, 0);
  if (wcharsCount == 0) {
    throw std::runtime_error("MultiByteToWideChar failed: " +
                             std::to_string(GetLastError()));
  }

  std::vector<wchar_t> wstr(wcharsCount);
  if (MultiByteToWideChar(CP_ACP, 0, asciiStr.c_str(), -1, &wstr[0],
                          wcharsCount) == 0) {
    throw std::runtime_error("MultiByteToWideChar failed: " +
                             std::to_string(GetLastError()));
  }

  // UTF-16 -> UTF-8
  int utf8Count =
      WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, NULL, 0, NULL, NULL);
  if (utf8Count == 0) {
    throw std::runtime_error("WideCharToMultiByte failed: " +
                             std::to_string(GetLastError()));
  }

  std::vector<char> utf8str(utf8Count);
  if (WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, &utf8str[0], utf8Count,
                          NULL, NULL) == 0) {
    throw std::runtime_error("WideCharToMultiByte failed: " +
                             std::to_string(GetLastError()));
  }

  return std::string(&utf8str[0]);
}
#endif

}  // namespace

ContentProfile::ContentProfile(const std::string& app, SDL_IOStream* stream)
    : program_name(app), ini_stream_(stream) {}

ContentProfile::~ContentProfile() = default;

void ContentProfile::LoadCommandLine(int32_t argc, char** argv) {
  for (int32_t i = 0; i < argc; ++i)
    args.push_back(argv[i]);

  for (int32_t i = 0; i < argc; i++) {
    if (std::string(argv[i]) == "test" || std::string(argv[i]) == "debug")
      game_debug = true;
    if (std::string(argv[i]) == "btest")
      game_battle_test = true;
  }
}

bool ContentProfile::LoadConfigure(const std::string& app) {
  std::unique_ptr<INIReader> reader = std::make_unique<INIReader>();
  if (ini_stream_) {
    reader = std::make_unique<INIReader>(ini_stream_, IniStreamReader);
    if (reader->ParseError())
      return false;
  } else {
    return false;
  }

  // Default
  script_path = reader->Get("Game", "Scripts", script_path);
  ReplaceStringWidth(script_path, '\\', '/');
  window_title = reader->Get("Game", "Title", window_title);
  rtp = reader->Get("Game", "RTP", rtp);

  if (!CheckValidUTF8(window_title.c_str())) {
#if defined(OS_WIN)
    window_title = ANSIFromUtf8(window_title);
#else
    window_title = "URGE Widget";
#endif
  }

  for (int32_t i = 1; i <= 3; ++i) {
    std::string rtp_key = "RTP";
    rtp_key += ('0' + i);

    if (rtp.empty())
      rtp = reader->Get("Game", rtp_key.c_str(), rtp);
  }

  // Engine
  api_version = static_cast<APIVersion>(reader->GetInteger(
      "Engine", "APIVersion", static_cast<int32_t>(api_version)));
  if (api_version == APIVersion::UNKNOWN) {
    if (!script_path.empty()) {
      api_version = APIVersion::RGSS1;

      const char* p = &script_path[script_path.size()];
      const char* head = &script_path[0];

      while (--p != head)
        if (*p == '.')
          break;

      if (!strcmp(p, ".rvdata"))
        api_version = APIVersion::RGSS2;
      else if (!strcmp(p, ".rvdata2"))
        api_version = APIVersion::RGSS3;
    }
  }
  default_font_path =
      reader->Get("Engine", "DefaultFontPath", default_font_path);
  ReplaceStringWidth(default_font_path, '\\', '/');
  i18n_xml_path =
      reader->Get("Engine", "I18nXMLPath", std::string(app + ".xml"));

  if (api_version == APIVersion::RGSS1)
    resolution = base::Vec2i(640, 480);
  if (api_version >= APIVersion::RGSS2)
    resolution = base::Vec2i(544, 416);
  resolution =
      GetVec2iFromString(reader->Get("Engine", "Resolution", ""), resolution);
  window_size =
      GetVec2iFromString(reader->Get("Engine", "WindowSize", ""), resolution);

  // GUI
  disable_settings =
      reader->GetBoolean("GUI", "DisableSettings", disable_settings);
  disable_fps_monitor =
      reader->GetBoolean("GUI", "DisableFPSMonitor", disable_fps_monitor);
  disable_reset = reader->GetBoolean("GUI", "DisableReset", disable_reset);

  // Renderer
  driver_backend = reader->Get("Renderer", "Backend", driver_backend);
  pipeline_default_sampler = reader->GetInteger(
      "Renderer", "PipelineDefaultSampler", pipeline_default_sampler);
  render_validation =
      reader->GetBoolean("Renderer", "RenderValidation", render_validation);
  frame_rate = reader->GetInteger("Renderer", "FrameRate",
                                  (api_version == APIVersion::RGSS1) ? 40 : 60);
  smooth_scale = reader->GetBoolean("Renderer", "SmoothScale", smooth_scale);
  allow_skip_frame =
      reader->GetBoolean("Renderer", "AllowSkipFrame", allow_skip_frame);
  fullscreen = reader->GetBoolean("Renderer", "Fullscreen", fullscreen);
  background_running =
      reader->GetBoolean("Renderer", "BackgroundRunning", background_running);

  // Platform
  debugging_console =
      reader->GetBoolean("Platform", "DebuggingConsole", debugging_console);
  disable_ime = reader->GetBoolean("Platform", "DisableIME", disable_ime);
  orientation = reader->Get("Platform", "Orientations", orientation);

  // Features
  disable_audio = reader->GetBoolean("Features", "DisableAudio", disable_audio);
  sprite_vertical_sort = reader->GetBoolean("Features", "SpriteVerticalSort",
                                            sprite_vertical_sort);

  if (ini_stream_)
    SDL_CloseIO(ini_stream_);

  return true;
}

}  // namespace content
