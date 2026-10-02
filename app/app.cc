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
#include "SDL3_image/SDL_image.h"

#include "app/platform/win32.h"

#include "core/config.h"
#include "core/filesystem.h"
#include "core/font_context.h"
#include "core/gpu.h"
#include "core/graphics.h"
#include "core/input.h"
#include "core/logger.h"
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

  // Components initialize
  try {
    auto io = new urge::IOService(argv[0]);
    urge::IOService::Reset(io);
    io->SetWritePath(base_dir);
    io->AddLoadPath(".", "/");

    auto config = new urge::Config(ini);
    urge::Config::Reset(config);

    auto input = new urge::Input(config->rgss_version);
    urge::Input::Reset(input);

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

  urge::Graphics::Reset(nullptr);
  urge::FontContext::Reset(nullptr);
  urge::Input::Reset(nullptr);
  urge::Config::Reset(nullptr);
  urge::IOService::Reset(nullptr);

  return 0;
}
