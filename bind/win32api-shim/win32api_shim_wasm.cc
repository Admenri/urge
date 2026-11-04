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

#include "SDL3/SDL_platform_defines.h"
#include "bind/win32api-shim/win32api_memory.h"
#include "bind/win32api-shim/win32api_shim.h"

#if defined(SDL_PLATFORM_EMSCRIPTEN)
namespace binding::win32api {
void RegisterWasm(Registry& registry) {
  RegisterProfiles(registry);
  RegisterProcess(registry);
  RegisterSDL(registry);
  memory::Register(registry);
  registry.AddNoOp("user32", "MoveWindow", 6);
  registry.AddNoOp("user32", "SetWindowPos", 7);
  registry.AddNoOp("user32", "SetCursorPos", 2);
  registry.AddNoOp("user32", "SetForegroundWindow", 1);
  registry.AddNoOp("user32", "ShowWindow", 2);
  registry.AddNoOp("kernel32", "SetPriorityClass", 2);
}
}  // namespace binding::win32api
#endif
