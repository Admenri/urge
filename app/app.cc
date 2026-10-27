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

/* Two hosts, chosen by URGE_RUNNER_MODEL.

   `poll` (the default everywhere the engine may block): this file has an
   ordinary `main()`.  It brings the engine up and then hands the thread to the
   game, which runs until the script ends; every `Graphics.update` is a whole
   frame, draining SDL's queue and sleeping out the rest of the game's slot.
   That is the shape RGSS has, and nothing has to be suspended between frames.

   `fiber` (the web, and anything else that cannot block): SDL's callback main
   loop.  SDL decides what a tick is -- an animation frame callback in a
   browser -- and `SDL_AppIterate` is one tick; the script runs on a stack of
   its own and hands the thread back at every Present.  Because SDL drains its
   queue into `SDL_AppEvent` there, `SDL_PollEvent` inside the frame would find
   nothing left: events are pushed onto `Graphics` instead. */

#if !defined(URGE_RUNNER_POLL)
#define SDL_MAIN_USE_CALLBACKS 1
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "app/platform/win32.h"

#if defined(OS_ANDROID)
#include "app/platform/android.h"
#endif

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
#include "core/input/touch.h"
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

/* Everything that has to survive between callbacks.  SDL hands this pointer
   back to every one of them, which is why none of it is a global: a host that
   restarts the engine -- or an embedder with two of them -- gets its own. */
struct AppState {
  std::string app;
  std::string base_dir;
  std::string ini;
  std::string log_path;

  urge::Config* config = nullptr;
  binding::BindingMain* binding = nullptr;
};

/* Puts the failure where it can be read: the console first, because a browser
   has no modal to put it in, then the message box, because a game started by
   double-clicking has no console behind it. */
void ReportFailure(const AppState& state,
                   const std::string& message,
                   bool with_log_hint) {
  LOGGER_ERROR("[Host] {}", message);

  std::string text = message;
  if (with_log_hint && !state.log_path.empty())
    text += "\n\nFull log: " + state.log_path;
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "URGE Core", text.c_str(),
                           nullptr);
}

/* Brings the engine down, in the reverse of the order it was built up.  Safe
   on a partially initialised state -- every step checks what it is about to
   tear down -- because it is the one path every exit takes. */
void Shutdown(AppState& state) {
  /* The interpreter goes first: its teardown runs the game's `at_exit` and
     finalisers, which may still reach for the engine. */
  delete state.binding;
  state.binding = nullptr;

  /* The settings are put on disk before anything goes away, so the next run
     starts where this one ended.  A run that never got as far as the
     configuration has nothing to write. */
  if (state.config) {
    urge::ExceptionState exception_state;
    state.config->Save(exception_state);
  }

  urge::Network::Reset(nullptr);
  urge::Graphics::Reset(nullptr);
  urge::FontContext::Reset(nullptr);
  urge::Touch::Reset(nullptr);
  urge::Mouse::Reset(nullptr);
  urge::Input::Reset(nullptr);
  urge::Audio::Reset(nullptr);
  urge::Config::Reset(nullptr);
  urge::IOService::Reset(nullptr);
  state.config = nullptr;

  if (g_log_stream) {
    urge::SetLogSink(nullptr, nullptr);
    SDL_CloseIO(g_log_stream);
    g_log_stream = nullptr;
  }

  TTF_Quit();
}

/* Everything `main()` used to do before the loop: the names, the filesystem,
   the components, the interpreter.  Returns false with `exception_state` set
   at the first step that fails. */
bool Initialize(AppState& state,
                int argc,
                char* argv[],
                urge::ExceptionState& exception_state) {
#if defined(OS_EMSCRIPTEN)
  /* A browser has no executable path to take the name from: argv[0] is
     whatever the module was started with, and the working directory is the
     root of the preloaded filesystem.  The name is fixed instead, which is
     what the shell and the preloaded `Game.ini` are written for. */
  state.app = "Game";
  state.base_dir = "/";
#elif defined(OS_ANDROID)
  /* The game lives inside the APK's `assets/`, which is a zip PhysicsFS cannot
     mount and RGSS cannot enumerate.  The activity unpacks it into the app's
     own directory on the first launch of a new build and hands the path back;
     from there this is the same layout a desktop game has -- the game and its
     saves share one directory.  The name is fixed for the same reason it is on
     the web: there is no executable file to take it from. */
  state.app = "Game";
  state.base_dir = platform::android::PrepareStorageRoot(exception_state);
  if (exception_state.HadException())
    return false;
#else
  state.app = argv[0];
  for (size_t i = 0; i < state.app.size(); ++i)
    if (state.app[i] == '\\')
      state.app[i] = '/';

  // Game directory (where the executable lives)
  state.base_dir = ".";
  auto last_sep = state.app.find_last_of('/');
  if (last_sep != std::string::npos) {
    state.base_dir = state.app.substr(0, last_sep);
    state.app = state.app.substr(last_sep + 1);
  }
#endif

  // The extension never belongs to the name: `Game.exe` and `Game` are the
  // same game, and both read `Game.ini`.
  auto extension_sep = state.app.find_last_of('.');
  if (extension_sep != std::string::npos)
    state.app = state.app.substr(0, extension_sep);
  state.ini = state.app + ".ini";

#if !defined(OS_EMSCRIPTEN)
  /* The game's directory becomes the process's working directory, and is made
     absolute before anything uses it.

     RGSS expects the two to be the same, and so does every way a file gets
     opened: the interpreter's own `File` resolves relative names against the
     working directory, PhysicsFS resolves the load path against it, and the
     write directory is set to the game's directory.  Started from a shortcut
     or a shell that is somewhere else, those three would point at two
     different places -- a save written by `save_data` would be invisible to
     `File.exist?`, and unreadable by `load_data` itself, because the file went
     to the write directory while the search path stayed where the shell was. */
  {
    std::error_code error;
    std::filesystem::current_path(state.base_dir, error);
    if (!error)
      state.base_dir = std::filesystem::current_path(error).generic_string();
  }
#endif

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
  TTF_Init();

  /* A game handed to a player is started by double-clicking it, so there is no
     console behind `stderr` and the message box is the only place a failure is
     ever seen -- until the click that closes it.  The same lines are mirrored
     into `<name>.log` beside the executable, which is what a bug report is made
     of.  A directory that refuses the file costs the mirror, never the run. */
#if !defined(OS_EMSCRIPTEN)
  state.log_path = state.base_dir + "/" + state.app + ".log";
  g_log_stream = SDL_IOFromFile(state.log_path.c_str(), "w");
  if (g_log_stream)
    urge::SetLogSink(&WriteLogFile, g_log_stream);
#endif

  auto io = new urge::IOService(exception_state, argv[0]);
  if (exception_state.HadException())
    return false;
  urge::IOService::Reset(io);
  /* One directory is both where the game is read from and where it writes:
     `state.base_dir` is absolute by now, and on the web it is the root of the
     module's in-memory filesystem, because that is where the preloaded game
     lands.  Naming it twice rather than using "." is what keeps the search
     path and the write directory from drifting apart. */
  io->SetWritePath(state.base_dir);
  io->AddLoadPath(state.base_dir, "/");

  /* A shipped game carries its resources in `<name>.acs`, named after the
     executable exactly the way the ini above is: `Game.exe` reads `Game.acs`.
     It goes in front of the plain directory, so the container wins wherever
     both have an entry and a development build that still ships loose files
     keeps working.  A build without the `admenri/` drop-in has no archiver, so
     this is skipped and nothing changes. */
  std::string archive_path = state.app + ".acs";
  if (urge::archive::Available() && io->Exists(archive_path))
    io->MountArchive(archive_path);

  state.config = new urge::Config(exception_state, state.ini);
  urge::Config::Reset(state.config);
  if (exception_state.HadException())
    return false;

  /* The switches the host was started with are recorded before anything can
     read them: the binding mirrors them onto the RGSS `$DEBUG` / `$TEST` /
     `$BTEST` globals, and a game may look for its own. */
  state.config->SetCommandLine(argc, argv);

// RTP reading
#if defined(_WIN32)
  auto add_rtp = [&](std::string key) {
    auto rtp_path = platform::win32::GetRTPPath(state.config->game.rgss, key);
    if (rtp_path.has_value()) {
      io->AddLoadPath(rtp_path.value(), "/");
      LOGGER_INFO("[RTP] Path: {}", rtp_path.value());
    }
  };
  add_rtp(state.config->game.rtp);
  add_rtp(state.config->game.rtp1);
  add_rtp(state.config->game.rtp2);
  add_rtp(state.config->game.rtp3);
#endif  // _WIN32

  auto input = new urge::Input(state.config->game.rgss);
  urge::Input::Reset(input);

  /* The mouse is created before the window exists: it reads the window of
     Graphics lazily, when a query or a warp runs, so it needs nothing of the
     window to be constructed, see Mouse.  The touch table is the same: it only
     holds what SDL reported, and asks Graphics where the picture is when a
     script reads a position. */
  auto mouse = new urge::Mouse();
  urge::Mouse::Reset(mouse);

  auto touch = new urge::Touch();
  urge::Touch::Reset(touch);

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

  /* Binding entry.  The constructor brings the interpreter up on the loop's
     own stack and loads the game's scripts without running them: `SDL_AppIterate`
     is what runs them, one frame at a time. */
  state.binding = new binding::BindingMain(exception_state);
  if (exception_state.HadException())
    return false;

  return true;
}

}  // namespace

#if defined(URGE_RUNNER_POLL)

int main(int argc, char* argv[]) {
  AppState state;

  urge::ExceptionState exception_state;
  if (!Initialize(state, argc, argv, exception_state)) {
    ReportFailure(state, exception_state.message(), true);
    Shutdown(state);
    return EXIT_FAILURE;
  }

  /* The game runs here until the script ends -- `exit`, a closed window, or an
     error.  There is no host loop around it: the frames are inside. */
  const binding::BindingMain::Tick outcome = state.binding->Run();
  const bool failed = outcome == binding::BindingMain::Tick::kFailed;

  if (failed)
    ReportFailure(state, state.binding->ErrorMessage(), true);

  Shutdown(state);
  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

#else  // URGE_RUNNER_FIBER

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  auto* state = new AppState();
  *appstate = state;

  urge::ExceptionState exception_state;
  if (!Initialize(*state, argc, argv, exception_state)) {
    ReportFailure(*state, exception_state.message(), true);
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* state = static_cast<AppState*>(appstate);
  if (!state || !state->binding)
    return SDL_APP_FAILURE;

  /* One tick: resume the game's fiber and let it run until it presents a frame.
     Whatever the script does in between is its own business -- the engine only
     ever sees the `Present` that ends a frame, and the fiber's end.

     Returning anything but `SDL_APP_CONTINUE` is the quit signal: SDL stops
     calling this, calls `SDL_AppQuit`, and takes the process down with the
     result it was given. */
  switch (state->binding->IterateMainLoop()) {
    case binding::BindingMain::Tick::kContinue:
      return SDL_APP_CONTINUE;

    case binding::BindingMain::Tick::kFinished:
      return SDL_APP_SUCCESS;

    case binding::BindingMain::Tick::kFailed:
      ReportFailure(*state, state->binding->ErrorMessage(), true);
      return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  /* SDL has already taken the event off its queue, so the engine cannot poll
     for it any more: it is handed over here and the frame's event pass reads
     it.  The quit is forwarded like everything else -- the engine turns it
     into the exit `Graphics.update` already raises, so the script unwinds and
     its `ensure` blocks run, instead of the loop being torn down under it. */
  if (event->type == SDL_EVENT_TERMINATING)
    return SDL_APP_SUCCESS;

  /* Lifecycle events (terminating, low memory, background) are dispatched from
     whichever thread pushed them, and the event list is the main thread's. */
  if (SDL_IsMainThread()) {
    if (auto* graphics = urge::Graphics::TryGet())
      graphics->PushEvent(*event);
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto* state = static_cast<AppState*>(appstate);
  if (!state)
    return;

  Shutdown(*state);
  delete state;
}

#endif  // URGE_RUNNER_POLL
