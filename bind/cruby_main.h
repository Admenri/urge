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

#include <string>

#include "core/common/exception.h"
#include "core/common/object.h"

namespace binding {

class BindingMain {
 public:
  /*! Creates the loop's stack, brings the interpreter up on it, and loads the
      game's scripts without running them.

      The interpreter has to be born on the stack it is going to run on: it
      records the machine stack's bounds at initialisation, and every
      stack-overflow check and collector scan is measured against them. */
  explicit BindingMain(urge::ExceptionState& exception_state);
  ~BindingMain();

  BindingMain(const BindingMain&) = delete;
  BindingMain& operator=(const BindingMain&) = delete;

  /*! What one tick of the loop decided. */
  enum class Tick {
    /*! The frame ran; the game is still going. */
    kContinue,
    /*! The script ran to its end.  The loop should quit. */
    kFinished,
    /*! The script ended on an error; `ErrorMessage` says which. */
    kFailed,
  };

  /*! One tick of the host's main loop: resumes the loop's stack and lets the
      game run until it presents a frame -- or until the script ends, which is
      the quit signal.

      A tick may run no frame at all, or two, when the host's clock and the
      game's rate do not divide: a 60 Hz display and a 40 fps game is the case
      that needs it. */
  Tick IterateMainLoop();

  /*! The failure the script ended with, already formatted with its backtrace;
      empty unless a tick returned `kFailed`. */
  const std::string& ErrorMessage() const;

 private:
  /*! Entry point of the loop's stack.  `script_loop::Start` is given this and
      the `BindingMain` to run it for. */
  static void LoopEntry(void* arg);

  void InitializeInterpreter(urge::ExceptionState& exception_state);
  void RunOnLoopStack();

  int error_state_ = 0;
};

}  // namespace binding
