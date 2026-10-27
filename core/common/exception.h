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

class ExceptionState final {
 public:
  enum Type {
    kUnknown = 0,
    kRGSSError,
    kGPUError,
    kIOError,
    kExitError,
    kResetError,
  };

  static constexpr std::size_t kMessageCapacity = 512;

  ExceptionState() = default;
  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  template <typename... Args>
  void Throw(Type type, std::string_view format, Args&&... args) {
    if (thrown_)
      return;
    type_ = type;
    thrown_ = true;
    if constexpr (sizeof...(Args) == 0) {
      Assign(format);
    } else {
      const std::string text =
          std::vformat(format, std::make_format_args(args...));
      Assign(text);
    }
  }

  void Throw(Type type) { Throw(type, std::string_view()); }

  bool HadException() const noexcept { return thrown_; }

  Type type() const noexcept { return type_; }

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

}
