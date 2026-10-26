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

namespace binding {

/*! The stack the game's scripts run on.

    SDL calls `SDL_AppIterate` once per tick and expects it to return, while the
    game's `loop { Graphics.update }` has to survive from one tick to the next.
    So the script runs on a stack of its own and hands the thread back at every
    `Present`.

    That is a plain context switch, deliberately not an interpreter feature: it
    is built on the coroutine backend the interpreter itself is compiled with
    (`coroutine/<arch>/Context.h`, whichever `COROUTINE_H` names), and it knows
    nothing about Ruby's Fibers.  A script is therefore free to create, resume,
    yield or transfer Fibers of its own -- the loop neither notices nor cares,
    and cannot be derailed by it.

    One loop per process, which is why handing the thread back is a free
    function.
*/
namespace script_loop {

/*! Entry point for the loop's stack.  It must not return: it ends by handing
    the thread back for the last time. */
using Entry = void (*)(void* arg);

/*! Allocates the stack and prepares the context.  Nothing runs until the first
    `Resume`. */
bool Start(Entry entry, void* arg);

/*! Host side: run until the loop hands the thread back. */
void Resume();

/*! Loop side: hand the thread back; returns when the host resumes again. */
void YieldToHost();

/*! Releases the stack.  `Resume` does nothing afterwards. */
void Finish();

/*! True between a successful `Start` and `Finish`. */
bool Started();

}  // namespace script_loop

}  // namespace binding
