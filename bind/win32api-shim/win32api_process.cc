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

#include "bind/win32api-shim/win32api_shim.h"

#include <unistd.h>
#include <atomic>

namespace binding::win32api {
namespace {
bool PseudoHandle(const Argument& argument, int32_t handle) {
  return !argument.buffer && (argument.number == handle ||
                              argument.number == static_cast<uint32_t>(handle));
}
Result InvalidHandle() {
  SetLastError(6);  // ERROR_INVALID_HANDLE
  return {};
}
}  // namespace

uint32_t CurrentProcessId() {
  // On the web this is Emscripten's virtual PID, matching Ruby Process.pid.
  return static_cast<uint32_t>(getpid());
}

uint32_t CurrentThreadId() {
  // Host pthread/SDL thread identifiers need not fit a Win32 DWORD. Allocate
  // positive 32-bit compatibility IDs, stable for each OS thread and never
  // recycled when a thread exits or the Ruby VM resets.
  static std::atomic<uint32_t> next{1};
  thread_local const uint32_t id = [] {
    auto candidate = next.load(std::memory_order_relaxed);
    while (candidate <= 0x7FFFFFFF) {
      if (next.compare_exchange_weak(candidate, candidate + 1,
                                     std::memory_order_relaxed))
        return candidate;
    }
    return uint32_t{0};
  }();
  if (!id)
    SetLastError(8);  // ERROR_NOT_ENOUGH_MEMORY
  return id;
}

void RegisterProcess(Registry& registry) {
  registry.Add(
      "kernel32", "GetCurrentProcessId", 0, 0,
      [](std::span<const Argument>) { return Result{CurrentProcessId()}; });
  registry.Add(
      "kernel32", "GetCurrentThreadId", 0, 0,
      [](std::span<const Argument>) { return Result{CurrentThreadId()}; });
  registry.Add("kernel32", "GetCurrentProcess", 0, 0,
               [](std::span<const Argument>) { return Result{-1}; });
  registry.Add("kernel32", "GetCurrentThread", 0, 0,
               [](std::span<const Argument>) { return Result{-2}; });
  registry.Add("kernel32", "GetProcessId", 1, 0,
               [](std::span<const Argument> args) {
                 return PseudoHandle(args[0], -1) ? Result{CurrentProcessId()}
                                                  : InvalidHandle();
               });
  registry.Add("kernel32", "GetThreadId", 1, 0,
               [](std::span<const Argument> args) {
                 return PseudoHandle(args[0], -2) ? Result{CurrentThreadId()}
                                                  : InvalidHandle();
               });
}
}  // namespace binding::win32api
