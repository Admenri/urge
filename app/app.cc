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
#include <string_view>

#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "app/platform/win32.h"

#include "core/system/archive.h"
#include "core/audio/audio.h"
#include "core/system/config.h"
#include "core/render/device.h"
#include "core/system/filesystem.h"
#include "core/font/font_context.h"
#include "core/graphics/graphics.h"
#include "core/input/input.h"
#include "core/common/logger.h"
#include "core/input/mouse.h"
#include "core/network/network.h"
#include "core/scene/plane.h"
#include "core/render/primitive.h"
#include "core/scene/sprite.h"
#include "core/render/uniform.h"

#include "bind/cruby_main.h"

namespace {

SDL_IOStream* g_log_stream = nullptr;

/* The mirror of `stderr` on disk.  It is written line by line and flushed at
   once: a run that dies in the interpreter never reaches the orderly shutdown
   below, so a line still sitting in the stream buffer would be the one line the
   report needed. */
void WriteLogFile(std::string_view line, void* user) {
  auto stream = static_cast<SDL_IOStream*>(user);
  if (!stream)
    return;
  SDL_WriteIO(stream, line.data(), line.size());
  SDL_FlushIO(stream);
}

}  // namespace


int main(int argc, char* argv[]) {
#if defined(OS_EMSCRIPTEN)
  /* A browser has no executable path to take the name from: argv[0] is
     whatever the module was started with, and the working directory is the
     root of the preloaded filesystem.  The name is fixed instead, which is
     what the shell and the preloaded `Game.ini` are written for. */
  std::string app = "Game";
  std::string base_dir = "/";
#else
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
#endif

  // The extension never belongs to the name: `Game.exe` and `Game` are the
  // same game, and both read `Game.ini`.
  auto extension_sep = app.find_last_of('.');
  if (extension_sep != std::string::npos)
    app = app.substr(0, extension_sep);
  std::string ini = app + ".ini";

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
  TTF_Init();

  /* A game handed to a player is started by double-clicking it, so there is no
     console behind `stderr` and the message box is the only place a failure is
     ever seen -- until the click that closes it.  The same lines are mirrored
     into `<name>.log` beside the executable, which is what a bug report is made
     of.  A directory that refuses the file costs the mirror, never the run. */
  std::string log_path;
#if !defined(OS_EMSCRIPTEN)
  log_path = base_dir + "/" + app + ".log";
  g_log_stream = SDL_IOFromFile(log_path.c_str(), "w");
  if (g_log_stream)
    urge::SetLogSink(&WriteLogFile, g_log_stream);
#endif

  // Components initialize.  Every constructor that can fail reports through
  // the state below, so the sequence stops at the first one that does.
  urge::Config* config = nullptr;
  urge::ExceptionState exception_state;

  auto run = [&]() {
    auto io = new urge::IOService(exception_state, argv[0]);
    if (exception_state.HadException())
      return;
    urge::IOService::Reset(io);
#if defined(OS_EMSCRIPTEN)
    /* The preloaded game is the root of the module's in-memory filesystem, so
       both paths are absolute: a relative one would be resolved against a
       working directory the browser does not really have. */
    io->SetWritePath(base_dir);
    io->AddLoadPath(base_dir, "/");
#else
    io->SetWritePath(base_dir);
    io->AddLoadPath(".", "/");
#endif

    /* A shipped game carries its resources in `<name>.acs`, named after the
       executable exactly the way the ini above is: `Game.exe` reads
       `Game.acs`.  It goes in front of the plain directory, so the container
       wins wherever both have an entry and a development build that still
       ships loose files keeps working.  A build without the `admenri/` drop-in
       has no archiver, so this is skipped and nothing changes. */
    std::string archive_path = app + ".acs";
    if (urge::archive::Available() && io->Exists(archive_path))
      io->MountArchive(archive_path);

    config = new urge::Config(exception_state, ini);
    urge::Config::Reset(config);

    /* The switches the host was started with are recorded before anything can
       read them: the binding mirrors them onto the RGSS `$DEBUG` / `$TEST` /
       `$BTEST` globals, and a game may look for its own. */
    config->SetCommandLine(argc, argv);

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
    auto font_context = new urge::FontContext(exception_state);
    urge::FontContext::Reset(font_context);

    auto graphics = new urge::Graphics(exception_state);
    urge::Graphics::Reset(graphics);

    /* The network service owns no window and no device: it is built after
       everything else and driven by the script (`Network.update`), not by
       Graphics, so nothing above has to know it exists.  It goes away before
       IOService because its workers read through the load paths. */
    auto network = new urge::Network();
    urge::Network::Reset(network);


    // Binding entry
    binding::BindingMain binding_main(exception_state);

  };

  run();

  if (exception_state.HadException()) {
    /* The message box is the desktop's way of saying what went wrong; in a
       browser there is no modal to put it in, and the console is the only place
       a failure can be read from, so it goes there first. */
    LOGGER_ERROR("[Host] {}", exception_state.message());
    std::string message = exception_state.message();
    if (g_log_stream)
      message += "\n\nFull log: " + log_path;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "URGE Core", message.c_str(),
                             nullptr);
  }

  /* The settings are put on disk before anything goes away, so the next run
     starts where this one ended.  A run that never got as far as the
     configuration has nothing to write. */
  if (config)
    config->Save(exception_state);

  urge::Network::Reset(nullptr);
  urge::Graphics::Reset(nullptr);
  urge::FontContext::Reset(nullptr);
  urge::Mouse::Reset(nullptr);
  urge::Input::Reset(nullptr);
  urge::Audio::Reset(nullptr);
  urge::Config::Reset(nullptr);
  urge::IOService::Reset(nullptr);

  if (g_log_stream) {
    urge::SetLogSink(nullptr, nullptr);
    SDL_CloseIO(g_log_stream);
    g_log_stream = nullptr;
  }

  TTF_Quit();
  SDL_Quit();

  return 0;
}
