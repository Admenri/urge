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

#include <cstddef>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

namespace urge {

/*! A failure a caller has to look at, in place of a C++ exception.

    The engine is built without exceptions (`/EHs-c-`, `-fno-exceptions`,
    `_HAS_EXCEPTIONS=0`), so "this call failed" travels as a value the caller
    passes in and inspects afterwards:

    ```cpp
    void Bitmap::Blt(URGE_EXCEPT, int32_t x, int32_t y, RefPtr<Bitmap> src) {
      if (!src) {
        exception_state.Throw(ExceptionState::kRGSSError, "blt: no source");
        return;
      }
      ...
    }
    ```

    Two properties are load bearing, and both come from the web build:

    * **No destructor.**  A frame that owns something with a destructor cannot
      raise: the compiler gives it a landing pad and the `longjmp` behind
      `rb_raise` is dispatched into it, which traps.  `ExceptionState` is a
      plain buffer plus two scalars, so a binding may declare one on its stack
      and raise after the last call without any cleanup in between.
    * **No storage of its own on the heap.**  The fixed 512 bytes are the
      whole of it, so the state itself can live in a frame that owns nothing.
      Formatting is the one exception, and only on the failure path: the
      library's type erased formatter is `std::vformat`, which answers a
      `std::string`, and that temporary is destroyed before `Throw` returns --
      long before the caller raises.

    Only the first `Throw` counts: a function that reports its own failure and
    then lets a deeper call report another keeps the first, which is the one
    closest to the caller's mistake.
*/
class ExceptionState final {
 public:
  //! What the failure is reported as on the Ruby side.
  enum Type {
    kUnknown = 0,
    kRGSSError,
    kGPUError,
    kIOError,
    kExitError,
    kResetError,
  };

  //! Longest message `Throw` keeps, terminator included.
  static constexpr std::size_t kMessageCapacity = 512;

  ExceptionState() = default;
  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  //! Records a failure; a no-op once one is already recorded.
  /*!
      `format` is a `std::format` string when arguments follow it, and a plain
      message when they do not -- `Throw(type, SDL_GetError())` copies the text
      verbatim, so a message that happens to contain braces is safe.
  */
  template <typename... Args>
  void Throw(Type type, std::string_view format, Args&&... args) {
    if (thrown_)
      return;
    type_ = type;
    thrown_ = true;
    if constexpr (sizeof...(Args) == 0) {
      Assign(format);
    } else {
      // `std::vformat` is the only type erased formatter the library has,
      // and it answers a `std::string`.  That temporary is the one
      // allocation this type makes, and only on the failure path -- the
      // state itself stays a plain buffer, which is what "no heap" means
      // here.
      const std::string text =
          std::vformat(format, std::make_format_args(args...));
      Assign(text);
    }
  }

  //! Records a failure with no message.
  void Throw(Type type) { Throw(type, std::string_view()); }

  //! Whether `Throw` has been called.
  bool HadException() const noexcept { return thrown_; }

  //! The type of the recorded failure; `kUnknown` while there is none.
  Type type() const noexcept { return type_; }

  //! The recorded message, never null.
  const char* message() const noexcept { return message_; }

 private:
  void Assign(std::string_view text) {
    const std::size_t size =
        text.size() < kMessageCapacity ? text.size() : kMessageCapacity - 1;
    if (size)
      std::memcpy(message_, text.data(), size);
    message_[size] = '\0';
  }

  Type type_ = kUnknown;
  bool thrown_ = false;
  char message_[kMessageCapacity] = {};
};

}  // namespace urge
