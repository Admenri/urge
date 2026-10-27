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
  explicit BindingMain(urge::ExceptionState& exception_state);
  ~BindingMain();

  BindingMain(const BindingMain&) = delete;
  BindingMain& operator=(const BindingMain&) = delete;

  enum class Tick {
    kContinue,
    kFinished,
    kFailed,
  };

#if defined(URGE_RUNNER_POLL)
  Tick Run();
#else
  Tick IterateMainLoop();
#endif

  const std::string& ErrorMessage() const;

 private:
  void InitializeInterpreter(urge::ExceptionState& exception_state);

#if !defined(URGE_RUNNER_POLL)
  static void LoopEntry(void* arg);

  void RunOnLoopStack();
#endif

  int error_state_ = 0;
};

}
