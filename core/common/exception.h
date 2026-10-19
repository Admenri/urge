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

#include <format>
#include <string>
#include <string_view>

namespace urge {

class Exception final {
 public:
  enum Type {
    kUnknown = 0,
    kRGSSError,
    kGPUError,
    kIOError,
    kExitError,
    kResetError,
  };

  template <typename... Args>
  explicit Exception(Type type, std::string_view format, Args&&... args) {
    type_ = type;
    if constexpr (sizeof...(Args) == 0) {
      message_.assign(format);
    } else {
      message_ = std::vformat(format, std::make_format_args(args...));
    }
  }

  Type type() const noexcept { return type_; }
  std::string message() const noexcept { return message_; }

 private:
  Type type_ = {};
  std::string message_;
};

}  // namespace urge
