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

#if !defined(URGE_RUNNER_POLL)

#include "bind/script_loop.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "ruby/config.h"

#if defined(OS_EMSCRIPTEN)
#include <emscripten/fiber.h>
#endif

extern "C" {
#include COROUTINE_H
}

#if defined(OS_EMSCRIPTEN)
#include <emscripten/stack.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace binding {
namespace script_loop {

namespace {

constexpr std::size_t kStackSize = 8u * 1024u * 1024u;

coroutine_context g_loop;
coroutine_context g_host;

void* g_stack = nullptr;
Entry g_entry = nullptr;
bool g_started = false;

#if defined(OS_EMSCRIPTEN)
uintptr_t g_stack_base = 0;
uintptr_t g_stack_end = 0;
#endif

void* AllocateStack() {
#if defined(_WIN32)
  return ::VirtualAlloc(nullptr, kStackSize, MEM_COMMIT | MEM_RESERVE,
                        PAGE_READWRITE);
#else
  return std::malloc(kStackSize);
#endif
}

void FreeStack(void* stack) {
#if defined(_WIN32)
  ::VirtualFree(stack, 0, MEM_RELEASE);
#else
  std::free(stack);
#endif
}

COROUTINE LoopEntry(coroutine_context* from, coroutine_context* self) {
  (void)from;
  g_entry(self->argument);
  for (;;) {
  }
}

}

bool Start(Entry entry, void* arg) {
  if (g_started)
    return false;

  g_stack = AllocateStack();
  if (!g_stack)
    return false;

  g_entry = entry;
  coroutine_initialize(&g_loop, reinterpret_cast<coroutine_start>(&LoopEntry),
                       g_stack, kStackSize);
  g_loop.argument = arg;

  coroutine_initialize_main(&g_host);

  g_started = true;
  return true;
}

void Resume() {
  if (!g_started)
    return;

#if defined(OS_EMSCRIPTEN)
  if (g_stack_base != 0) {
    g_loop.state.stack_base = reinterpret_cast<void*>(g_stack_base);
    g_loop.state.stack_limit = reinterpret_cast<void*>(g_stack_end);
  }
#endif

  coroutine_transfer(&g_host, &g_loop);
}

void YieldToHost() {
#if defined(OS_EMSCRIPTEN)
  g_stack_base = emscripten_stack_get_base();
  g_stack_end = emscripten_stack_get_end();
#endif

  coroutine_transfer(&g_loop, &g_host);
}

void Finish() {
  if (!g_started)
    return;

  g_started = false;
  FreeStack(g_stack);
  g_stack = nullptr;
}

bool Started() {
  return g_started;
}

}
}

#endif
