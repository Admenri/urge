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

#pragma once

#include <cstdint>
#include <string>

namespace platform {
namespace profiler {

/* A sampling profiler that writes a report meant to be read by someone who is
   about to change the source.

   PGO answers "what does the compiler do with this code".  This answers the
   question that has to be settled first: which code is actually being run, how
   often, from where, and how much of the frame it owns.  The report is plain
   text -- a list of functions with self and inclusive time, a table of hot
   source lines, and the call tree those numbers add up through -- because the
   reader is a person with the file open, not a build step.

   It works by suspending the engine's own thread on a timer and reading the
   stack out of it.  Nothing is instrumented, so what is measured is the code
   as it ships; the price is that the numbers are statistical -- at 500 Hz a
   frame at 60 fps is sampled about eight times, which is enough to rank
   functions and not enough to time one call.

   The sampling thread never allocates while the engine is suspended: the frames
   of a stack are copied into a buffer reserved at start-up, and every symbol
   lookup happens after the run, when nothing is frozen.  A sampler that asked
   the heap for memory while the engine held the heap lock would deadlock the
   process it is measuring.

   Windows only: it unwinds with the same table-driven unwinder the system uses
   (`RtlVirtualUnwind`) and resolves addresses with dbghelp, both of which need
   a PE image and a PDB.  Everything else in the engine is portable; a profile
   of the engine is not.

   Two switches, read by the host before it installs this, and one hotkey:

     Game.ini  [Debug] Profiler = true | false
     Game.ini  [Debug] ProfilerInterval = <microseconds between samples>
     URGE_PROFILER=1 / URGE_PROFILER_INTERVAL=<us> / URGE_PROFILER_DIR=<dir>

   F4 starts and stops it while the game runs.  Stopping is what writes the
   report, so a run can be sampled in the middle of the scene that matters
   instead of from start to exit. */

struct Options {
  /* How long the sampler waits between two stacks.  A shorter interval buys
     resolution and pays for it in overhead: each sample suspends the engine
     thread twice and walks the stack while it is stopped. */
  int interval_us = 2000;

  /* Frames kept per stack.  Recursion in the interpreter is deeper than this;
     a stack that hits the limit is reported as truncated rather than dropped.
     96 reaches past a dozen nested Ruby blocks, which is where the callers that
     matter are -- what is left below it is `main` and the CRT. */
  int max_depth = 96;

  /* Total frames the run may record, across every stack.  8M frames is 64 MiB
     and covers about three minutes of a busy 400 Hz run at the depth above;
     when it is full the sampler stops adding and counts what it dropped. */
  std::uint32_t max_frames = 8u * 1024u * 1024u;
};

/* Starts the sampler's thread.  Must be called on the thread that runs the
   game, because that is the thread it samples.  `utf8_output_dir` is where the
   reports land and `utf8_prefix` is what they are named after, both in UTF-8
   like every other path the host hands to a component.  Returns false when the
   thread could not be created; the engine runs either way. */
bool Install(const std::string& utf8_output_dir,
             const std::string& utf8_prefix,
             const Options& options,
             bool start_now);

/* Starts sampling if it is stopped, stops it and writes the report if it is
   running.  This is what F4 does. */
void Toggle();

bool Running();

/* Stops sampling and writes the report.  Does nothing when nothing was
   sampled, which is what makes it safe to call from the shutdown path whether
   or not the run was profiled. */
void Stop();

/* Stops the thread and joins it.  Call after Stop, before the components the
   report reads are torn down. */
void Uninstall();

}  // namespace profiler
}  // namespace platform
