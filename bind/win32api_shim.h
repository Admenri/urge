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
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace binding::win32api {

// Values have Win32 widths regardless of the host ABI. Buffers always carry
// their Ruby String length; numeric pointers are opaque tokens, never host
// addresses. A backend must explicitly interpret every handle it accepts.
struct Argument {
  int64_t number = 0;
  char* data = nullptr;
  size_t size = 0;
  bool buffer = false;
  bool null() const { return !buffer && number == 0; }
  std::string_view text() const;
};

struct Result {
  int64_t number = 0;
  int destination = -1;
  std::string text;
  std::string error;
  static Result Failure(std::string message);
};

using Handler = Result (*)(std::span<const Argument>);
struct Function {
  size_t arity;
  uint32_t writable;
  Handler handler;
};

// Registration happens once before resolution. The resulting registry is
// immutable; each platform contributes handlers without changing Ruby's ABI.
class Registry {
 public:
  void Add(const std::string& library,
           const std::string& name,
           size_t arity,
           uint32_t writable,
           Handler handler);
  const Function* Find(std::string library, const std::string& name) const;

 private:
  std::unordered_map<std::string, Function> functions_;
};

const Registry& GetRegistry();
void RegisterPlatform(Registry& registry);
void RegisterMacOS(Registry& registry);
void RegisterProfiles(Registry& registry);
void RegisterSDL(Registry& registry);
uint32_t LastError();
void SetLastError(uint32_t value);

}  // namespace binding::win32api
