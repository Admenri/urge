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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#include "SDL3/SDL_main.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "app/platform/win32.h"

#include "core/audio.h"
#include "core/config.h"
#include "core/device.h"
#include "core/filesystem.h"
#include "core/font_context.h"
#include "core/graphics.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/mouse.h"
#include "core/plane.h"
#include "core/primitive.h"
#include "core/sprite.h"
#include "core/uniform.h"

#include "bind/cruby_main.h"

int main(int argc, char* argv[]) {
  // App name
  std::string app(argv[0]);
  for (size_t i = 0; i < app.size(); ++i)
    if (app[i] == '\\')
      app[i] = '/';

  // Game directory (where the executable lives)
  std::string base_dir = ".";
  auto last_sep = app.find_last_of('/');
  if (last_sep != std::string::npos) {
    base_dir = app.substr(0, last_sep);
    app = app.substr(last_sep + 1);
  }

  last_sep = app.find_last_of('.');
  if (last_sep != std::string::npos)
    app = app.substr(0, last_sep);
  std::string ini = app + ".ini";

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
  TTF_Init();

  // Components initialize
  urge::Config* config = nullptr;
  try {
    auto io = new urge::IOService(argv[0]);
    urge::IOService::Reset(io);
    io->SetWritePath(base_dir);
    io->AddLoadPath(".", "/");

    config = new urge::Config(ini);
    urge::Config::Reset(config);

// RTP reading
#if defined(_WIN32)
    auto add_rtp = [&](std::string key) {
      auto rtp_path = platform::win32::GetRTPPath(config->game.rgss, key);
      if (rtp_path.has_value()) {
        io->AddLoadPath(rtp_path.value(), "/");
        LOGGER_INFO("[RTP] Path: {}", rtp_path.value());
      }
    };
    add_rtp(config->game.rtp);
    add_rtp(config->game.rtp1);
    add_rtp(config->game.rtp2);
    add_rtp(config->game.rtp3);
#endif  // _WIN32

    auto input = new urge::Input(config->game.rgss);
    urge::Input::Reset(input);

    /* The mouse is created before the window exists: it reads the window of
       Graphics lazily, when a query or a warp runs, so it needs nothing of the
       window to be constructed, see Mouse. */
    auto mouse = new urge::Mouse();
    urge::Mouse::Reset(mouse);

    /* The mixer is built on the audio settings the file carried, once the load
       paths are mounted -- it reads its tracks through them -- and before
       Graphics, whose every frame drives it, see Graphics::Update(). */
    auto audio = new urge::Audio();
    urge::Audio::Reset(audio);

    /* The font service is set up after the load paths are mounted -- it reads
       the font directory off them -- and before anything that can draw text. */
    auto font_context = new urge::FontContext();
    urge::FontContext::Reset(font_context);

    auto graphics = new urge::Graphics();
    urge::Graphics::Reset(graphics);

    // Binding entry
    binding::BindingMain binding_main;
  } catch (const urge::Exception& exc) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "URGE Core",
                             exc.message().c_str(), nullptr);
  } catch (const std::exception& exc) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "URGE Core", exc.what(),
                             nullptr);
  }

  /* The settings are put on disk before anything goes away, so the next run
     starts where this one ended.  A run that never got as far as the
     configuration has nothing to write. */
  if (config)
    config->Save();

  urge::Graphics::Reset(nullptr);
  urge::FontContext::Reset(nullptr);
  urge::Mouse::Reset(nullptr);
  urge::Input::Reset(nullptr);
  urge::Audio::Reset(nullptr);
  urge::Config::Reset(nullptr);
  urge::IOService::Reset(nullptr);

  TTF_Quit();
  SDL_Quit();

  return 0;
}
