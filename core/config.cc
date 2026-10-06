// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/config.h"

#include <string>
#include <utility>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_platform_defines.h"
#include "SDL3/SDL_stdinc.h"

#include "core/exception.h"
#include "core/filesystem.h"

#if defined(SDL_PLATFORM_WIN32)
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace urge {

namespace {

static void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

std::vector<std::string> SplitKeys(const std::string& line) {
  std::vector<std::string> names;
  size_t start = 0;
  for (;;) {
    const size_t comma = line.find(',', start);
    const std::string name =
        line.substr(start, comma == std::string::npos ? std::string::npos
                                                      : comma - start);
    if (!name.empty())
      names.push_back(name);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return names;
}

int32_t ParseKeyName(const std::string& name) {
  const auto scancode = SDL_GetScancodeFromName(name.c_str());
  if (scancode != SDL_SCANCODE_UNKNOWN)
    return static_cast<int32_t>(scancode);
  try {
    return std::stoi(name);
  } catch (...) {
    return static_cast<int32_t>(SDL_SCANCODE_UNKNOWN);
  }
}

std::string KeyNameOf(int32_t keycode) {
  const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(keycode));
  return (name && *name) ? std::string(name) : std::to_string(keycode);
}

bool IsUTF8Text(const std::string& text) {
  const char* cursor = text.c_str();
  size_t left = text.size();
  while (left > 0 && *cursor != '\0')
    if (SDL_StepUTF8(&cursor, &left) == SDL_INVALID_UNICODE_CODEPOINT)
      return false;
  return true;
}

std::string AnsiToUTF8(const std::string& text) {
#if defined(SDL_PLATFORM_WIN32)
  const int length = MultiByteToWideChar(
      CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0)
    return text;

  std::wstring wide(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), length);
#else
  const size_t length = std::mbstowcs(nullptr, text.c_str(), 0);
  if (length == static_cast<size_t>(-1))
    return text;

  std::wstring wide(length, L'\0');
  std::mbstowcs(wide.data(), text.c_str(), length);
#endif

  char* utf8 = SDL_iconv_wchar_utf8(wide.c_str());
  if (!utf8)
    return text;

  std::string result(utf8);
  SDL_free(utf8);
  return result;
}

std::string NormalizeUTF8(const std::string& text) {
  if (IsUTF8Text(text))
    return text;
  return AnsiToUTF8(text);
}

}  // namespace

Config::Config(std::string inifile) : inifile_(std::move(inifile)) {
  auto stream = IOService::Get().OpenReadRaw(inifile_);
  if (stream) {
    size_t data_size = 0;
    auto* data_ptr = SDL_LoadFile_IO(stream, &data_size, true);
    std::string data(static_cast<char*>(data_ptr), data_size);
    parser_.LoadFromString(data);
    SDL_free(data_ptr);
  }

  Load();
}

void Config::Save() {
  parser_.Set("Game", "RGSS", game.rgss);
  parser_.Set("Game", "Scripts", game.scripts);
  parser_.Set("Game", "Title", game.title);
  parser_.Set("Game", "RTP", game.rtp);
  parser_.Set("Game", "RTP1", game.rtp1);
  parser_.Set("Game", "RTP2", game.rtp2);
  parser_.Set("Game", "RTP3", game.rtp3);

  parser_.Set("Audio", "Soundfont", audio.soundfont);
  parser_.Set("Audio", "MasterVolume", audio.master_volume);
  parser_.Set("Audio", "BGMVolume", audio.bgm_volume);
  parser_.Set("Audio", "BGSVolume", audio.bgs_volume);
  parser_.Set("Audio", "MEVolume", audio.me_volume);
  parser_.Set("Audio", "SEVolume", audio.se_volume);
  parser_.Set("Audio", "OutputDevice", audio.output_device);

  parser_.Set("Display", "Width", display.width);
  parser_.Set("Display", "Height", display.height);
  parser_.Set("Display", "VSync", display.vsync);
  parser_.Set("Display", "Fullscreen", display.fullscreen);
  parser_.Set("Display", "FrameSkip", display.frame_skip);

  parser_.Set("GFX", "Backend", gfx.backend);

  parser_.RemoveSection("KeyBinding");
  for (const auto& binding : key_bindings) {
    std::string line = parser_.Get("KeyBinding", binding.first);
    if (!line.empty())
      line += ',';
    line += KeyNameOf(binding.second);
    parser_.Set("KeyBinding", binding.first, line);
  }

  auto* stream = IOService::Get().OpenWrite(inifile_);
  if (!stream)
    return;
  const std::string text = parser_.ToString();
  SDL_WriteIO(stream, text.data(), text.size());
  SDL_CloseIO(stream);
}

void Config::Load() {
  game.rgss = parser_.GetInt("Game", "RGSS", game.rgss);
  game.scripts = parser_.Get("Game", "Scripts", game.scripts);
  ReplaceStringWidth(game.scripts, '\\', '/');
  game.title = NormalizeUTF8(parser_.Get("Game", "Title", game.title));
  game.rtp = parser_.Get("Game", "RTP", game.rtp);
  game.rtp1 = parser_.Get("Game", "RTP1", game.rtp);
  game.rtp2 = parser_.Get("Game", "RTP2", game.rtp);
  game.rtp3 = parser_.Get("Game", "RTP3", game.rtp);

  if (game.rgss == 0) {
    std::string ext;
    auto dot = game.scripts.find_last_of('.');
    if (dot != std::string::npos)
      ext = game.scripts.substr(dot);
    else
      ext = game.scripts;

    if (ext == ".rxdata")
      game.rgss = 1;
    else if (ext == ".rvdata")
      game.rgss = 2;
    else if (ext == ".rvdata2")
      game.rgss = 3;
  }

  audio.soundfont = parser_.Get("Audio", "Soundfont", audio.soundfont);
  audio.master_volume =
      parser_.GetInt("Audio", "MasterVolume", audio.master_volume);
  audio.bgm_volume = parser_.GetInt("Audio", "BGMVolume", audio.bgm_volume);
  audio.bgs_volume = parser_.GetInt("Audio", "BGSVolume", audio.bgs_volume);
  audio.me_volume = parser_.GetInt("Audio", "MEVolume", audio.me_volume);
  audio.se_volume = parser_.GetInt("Audio", "SEVolume", audio.se_volume);
  audio.output_device =
      parser_.Get("Audio", "OutputDevice", audio.output_device);

  display.width = parser_.GetInt("Display", "Width", xp() ? 640 : 544);
  display.height = parser_.GetInt("Display", "Height", xp() ? 480 : 416);
  display.vsync = parser_.GetBool("Display", "VSync", display.vsync);
  display.fullscreen =
      parser_.GetBool("Display", "Fullscreen", display.fullscreen);
  display.frame_skip =
      parser_.GetBool("Display", "FrameSkip", display.frame_skip);

  gfx.backend = parser_.Get("GFX", "Backend", gfx.backend);

  key_bindings.clear();
  for (const auto& symbol : parser_.GetKeys("KeyBinding"))
    for (const auto& name : SplitKeys(parser_.Get("KeyBinding", symbol)))
      key_bindings.push_back({symbol, ParseKeyName(name)});
}

}  // namespace urge
