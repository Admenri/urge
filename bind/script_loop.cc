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

#include "bind/script_loop.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

/* `COROUTINE_H` is the backend the interpreter was built with -- the CMake
   build picks it per platform and writes the name into `ruby/config.h`.  Going
   through it rather than naming a directory here is what keeps this file and
   `cont.c` using the same `struct coroutine_context`: a mismatch would be a
   silent stack corruption, not a compile error.

   The trampoline and the transfer are assembly with C linkage -- the
   interpreter calls them from C -- so the header has to be read as C.  Its own
   includes are pulled in above and here, outside the block: they are C++
   headers (Emscripten's `em_asm.h` defines templates) and the guards make them
   no-ops by the time the block reads them again. */
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

/* 8 MiB.  A frame reaches from the script through the whole renderer, and on
   the web the same allocation also holds the Asyncify spill -- the interpreter
   splits it down the middle there, leaving 4 MiB of each. */
constexpr std::size_t kStackSize = 8u * 1024u * 1024u;

coroutine_context g_loop;
coroutine_context g_host;

void* g_stack = nullptr;
Entry g_entry = nullptr;
bool g_started = false;

#if defined(OS_EMSCRIPTEN)
/* The stack the loop is *actually* on, which is not always the one it started
   on: a script may present from a Fiber of its own, and then the stack pointer
   that gets saved belongs to that fiber.  Recording the bounds at the hand-off
   and putting them back before resuming is what keeps the collector scanning
   the right range -- it reads them from `emscripten_stack_get_base` and
   `emscripten_scan_stack`, both of which the swap itself would have set to this
   context's own stack. */
uintptr_t g_stack_base = 0;
uintptr_t g_stack_end = 0;
#endif

/* The interpreter reads the machine stack's bounds from the allocation it is
   *initialised* on (`VirtualQuery` in thread_win32.c, the wasm stack on the
   web), and it is going to run on this one -- so this one has to look like a
   stack.  A block inside a CRT heap segment would report the heap's extent
   instead, which makes the collector scan megabytes of unrelated memory and
   makes every stack-overflow check fire at once. */
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

/* `coroutine_start`, which the backend calls the first time the context is
   resumed.  It must not return, so the entry is given the whole stack and
   whatever it leaves behind is not ours to unwind. */
COROUTINE LoopEntry(coroutine_context* from, coroutine_context* self) {
  (void)from;
  g_entry(self->argument);
  for (;;) {
  }
}

}  // namespace

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

  /* The host's own context: "the stack we were called on".  The backend fills
     it in on the first transfer, so nothing has to be known about it here. */
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

}  // namespace script_loop
}  // namespace binding
