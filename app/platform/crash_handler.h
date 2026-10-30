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

namespace platform {
namespace crash {

/* Turns a fatal fault into two files on disk: a minidump a debugger can open,
   and a plain-text report that says what happened without one.

   A game is shipped as a bare executable, so the window closing is the whole
   story a player can tell: no stack, no line, no module.  The report is written
   for that reader -- the exception, the faulting module and offset, the call
   stack with symbols when a PDB is beside the executable, and the paths of the
   log and the dump -- and the dump is written for a developer, who can attach
   WinDbg and walk every thread of the process.

   Windows only.  The files land in `<exe dir>/crash/`, falling back to
   `%LOCALAPPDATA%\URGE\crash` and then to `%TEMP%` when the game directory
   refuses them (an install under `Program Files`, a read-only disc).  Two
   switches override that, read once at install time:

     URGE_CRASH_DUMP=0     do nothing at all
     URGE_CRASH_DIR=<dir>  write there instead
     URGE_CRASH_FULL=1     dump the whole address space, not just the stacks
     URGE_CRASH_DIALOG=0   never show the message box

   Call it as early as the host can: a fault during engine start-up is exactly
   the kind that has no log line in front of it. */
void Install();

/* Records the log the host mirrors itself into, so the report can point the
   reader at it.  The path is UTF-8, which is what the rest of the host passes
   to SDL.  A run that never opened a log simply leaves it unset. */
void SetLogPath(const char* utf8_path);

}  // namespace crash
}  // namespace platform
