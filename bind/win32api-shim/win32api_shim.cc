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

#include "bind/win32api-shim/win32api_shim.h"

#include <utility>

namespace binding::win32api {
namespace {
thread_local uint32_t last_error = 0;
std::string LibraryName(std::string name) {
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string::npos)
    name.erase(0, slash + 1);
  for (char& ch : name)
    if (ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';
  if (name.ends_with(".dll"))
    name.resize(name.size() - 4);
  return name;
}
}  // namespace

std::string_view Argument::text() const {
  if (!buffer)
    return {};
  const std::string_view value(data, size);
  return value.substr(0, value.find('\0'));
}
Result Result::Failure(std::string message) {
  Result result;
  result.error = std::move(message);
  return result;
}
void Registry::Add(const std::string& library,
                   const std::string& name,
                   size_t arity,
                   uint32_t writable,
                   Handler handler) {
  functions_.emplace(LibraryName(library) + ':' + name,
                     Function{arity, writable, handler});
}
const Function* Registry::Find(std::string library,
                               const std::string& name) const {
  const auto found =
      functions_.find(LibraryName(std::move(library)) + ':' + name);
  return found == functions_.end() ? nullptr : &found->second;
}
const Registry& GetRegistry() {
  static const Registry registry = [] {
    Registry result;
    result.Add("kernel32", "GetLastError", 0, 0,
               [](std::span<const Argument>) { return Result{LastError()}; });
    result.Add("kernel32", "SetLastError", 1, 0,
               [](std::span<const Argument> args) {
                 SetLastError(static_cast<uint32_t>(args[0].number));
                 return Result{};
               });
    RegisterPlatform(result);
    return result;
  }();
  return registry;
}
uint32_t LastError() {
  return last_error;
}
void SetLastError(uint32_t value) {
  last_error = value;
}
}  // namespace binding::win32api
