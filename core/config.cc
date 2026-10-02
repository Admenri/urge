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

#include "core/exception.h"
#include "core/filesystem.h"

namespace urge {

static void ReplaceStringWidth(std::string& str, char before, char after) {
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == before)
      str[i] = after;
}

Config::Config(std::string inifile) {
  // Load the ini through the virtual file system (explicit extension)
  auto stream = IOService::Get().OpenReadRaw(inifile);
  {
    size_t data_size = 0;
    auto* data_ptr = SDL_LoadFile_IO(stream, &data_size, true);
    std::string data(static_cast<char*>(data_ptr), data_size);
    parser_.LoadFromString(data);
    SDL_free(data_ptr);
  }

  game.rgss = parser_.GetInt("Game", "RGSS", game.rgss);
  game.scripts = parser_.Get("Game", "Scripts", game.scripts);
  ReplaceStringWidth(game.scripts, '\\', '/');
  game.title = parser_.Get("Game", "Title", game.title);
  game.rtp = parser_.Get("Game", "RTP", game.rtp);
  game.rtp1 = parser_.Get("Game", "RTP1", game.rtp);
  game.rtp2 = parser_.Get("Game", "RTP2", game.rtp);
  game.rtp3 = parser_.Get("Game", "RTP3", game.rtp);

  // Auto-detect the RGSS version from the scripts file extension when
  // it is not explicitly configured (rgss_version == 0).
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

  window.width = parser_.GetInt("Window", "Width", xp() ? 640 : 544);
  window.height = parser_.GetInt("Window", "Height", xp() ? 480 : 416);

  gfx.backend = parser_.Get("GFX", "Backend", gfx.backend);
}

}  // namespace urge
