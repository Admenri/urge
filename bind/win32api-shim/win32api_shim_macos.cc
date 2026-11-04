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
#if defined(SDL_PLATFORM_MACOS)
#include <sys/resource.h>
#include <cerrno>
#include "core/common/logger.h"
namespace binding::win32api {
namespace {
Result Fail(uint32_t code) {
  SetLastError(code);
  return {};
}
Result Priority(std::span<const Argument> args) {
  if (args[0].number != -1 && args[0].number != 0xFFFFFFFFLL)
    return Fail(6);
  int nice;
  switch (args[1].number) {
    case 0x20:
      nice = 0;
      break;
    case 0x40:
      nice = 19;
      break;
    case 0x80:
      nice = -10;
      break;
    case 0x4000:
      nice = 5;
      break;
    case 0x8000:
      nice = -5;
      break;
    default:
      return Fail(87);
  }
  if (setpriority(PRIO_PROCESS, 0, nice) == 0)
    return Result{1};
  // Raising priority commonly needs privileges. Report failure honestly; never
  // request elevation or pretend that the requested priority was applied.
  const int error = errno;
  LOGGER_DEBUG("[Win32API] SetPriorityClass: setpriority failed ({})", error);
  return Fail(error == EPERM || error == EACCES ? 5 : 87);
}
}  // namespace
void RegisterMacOS(Registry& registry) {
  RegisterProfiles(registry);
  RegisterSDL(registry);
  memory::Register(registry);
  registry.Add("kernel32", "SetPriorityClass", 2, 0, Priority);
}
}  // namespace binding::win32api
#endif
