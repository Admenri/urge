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

#include "app/platform/win32.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <io.h>
#include <windows.h>


namespace {

const char* kRegPathXP = "SOFTWARE\\Enterbrain\\RGSS\\RTP";
const char* kRegPathVX = "SOFTWARE\\Enterbrain\\RGSS2\\RTP";
const char* kRegPathVXA = "SOFTWARE\\Enterbrain\\RGSS3\\RTP";

bool OpenRegistryKey(HKEY root, const char* subKey, HKEY* hKey) {
  LONG ret = RegOpenKeyExA(root, subKey, 0, KEY_READ | KEY_WOW64_32KEY, hKey);
  if (ret == ERROR_SUCCESS)
    return true;

  ret = RegOpenKeyExA(root, subKey, 0, KEY_READ | KEY_WOW64_64KEY, hKey);
  return ret == ERROR_SUCCESS;
}

bool ReadRegistryString(HKEY root,
                        const char* subKey,
                        const char* valueName,
                        std::string& value) {
  HKEY hKey = NULL;
  if (!OpenRegistryKey(root, subKey, &hKey))
    return false;

  DWORD type = 0;
  DWORD size = 0;
  LONG ret = RegQueryValueExA(hKey, valueName, NULL, &type, NULL, &size);
  if (ret != ERROR_SUCCESS) {
    RegCloseKey(hKey);
    return false;
  }

  if (type != REG_SZ && type != REG_EXPAND_SZ) {
    RegCloseKey(hKey);
    return false;
  }

  // Registry strings are not guaranteed to be null-terminated; reserve one
  // extra byte and terminate explicitly before reading it as a C string.
  std::vector<char> buffer(size + 1, '\0');
  ret = RegQueryValueExA(hKey, valueName, NULL, &type,
                         reinterpret_cast<LPBYTE>(buffer.data()), &size);
  RegCloseKey(hKey);
  if (ret != ERROR_SUCCESS)
    return false;

  value.assign(buffer.data());
  if (type == REG_EXPAND_SZ) {
    DWORD len = ExpandEnvironmentStringsA(value.c_str(), NULL, 0);
    if (len > 0) {
      std::vector<char> expanded(len);
      ExpandEnvironmentStringsA(value.c_str(), expanded.data(), len);
      value.assign(expanded.data());
    }
  }

  return true;
}

std::string WStringToUTF8(const std::wstring& wstr) {
  if (wstr.empty())
    return std::string();

  int32_t size_needed = WideCharToMultiByte(
      CP_UTF8, 0, &wstr[0], (int32_t)wstr.size(), NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int32_t)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

bool ReopenStandardStream(FILE* stream,
                          int descriptor,
                          const char* path,
                          const char* mode) {
  // A GUI CRT can report -2 for a stream while keeping its fd reserved.
  // Free that slot first so freopen and Ruby agree on exactly fd 0/1/2.
  if (_fileno(stream) != descriptor)
    _close(descriptor);
  return std::freopen(path, mode, stream) != nullptr &&
         _fileno(stream) == descriptor;
}

bool RebindStandardStreams(const char* input_path, const char* output_path) {
  const bool input = ReopenStandardStream(stdin, 0, input_path, "r");
  const bool output = ReopenStandardStream(stdout, 1, output_path, "w+");
  const bool error = ReopenStandardStream(stderr, 2, output_path, "w+");
  if (!input || !output || !error)
    return false;

  const bool handles =
      SetStdHandle(STD_INPUT_HANDLE,
                   reinterpret_cast<HANDLE>(_get_osfhandle(0))) &&
      SetStdHandle(STD_OUTPUT_HANDLE,
                   reinterpret_cast<HANDLE>(_get_osfhandle(1))) &&
      SetStdHandle(STD_ERROR_HANDLE,
                   reinterpret_cast<HANDLE>(_get_osfhandle(2)));
  if (!handles)
    return false;

  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::cin.clear();
  std::cout.clear();
  std::cerr.clear();
  std::clog.clear();
  return true;
}

}  // namespace

namespace platform {
namespace win32 {

std::filesystem::path GetExecutablePath() {
  // `GetModuleFileNameW` truncates and reports the buffer size instead of the
  // path when the buffer is too small, so the loop grows until it fits.  The
  // limit is the one Windows itself uses for a path.
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0)
      return {};
    if (length < buffer.size()) {
      buffer.resize(length);
      return std::filesystem::path(buffer);
    }
    if (buffer.size() >= 32768)
      return {};
    buffer.resize(buffer.size() * 2);
  }
}

bool InitializeStandardStreams() {
  return RebindStandardStreams("NUL", "NUL");
}

bool OpenConsole() {
  // ERROR_ACCESS_DENIED also means the process already owns a console (for
  // example, one supplied by a debugger). Never attach to the parent's.
  if (!AllocConsole() && GetLastError() != ERROR_ACCESS_DENIED)
    return false;

  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  if (!RebindStandardStreams("CONIN$", "CONOUT$"))
    return false;

  return true;
}

std::optional<std::string> GetRTPPath(int32_t version, std::string key) {
  const char* kRegPath = nullptr;
  if (version == 1)
    kRegPath = kRegPathXP;
  if (version == 2)
    kRegPath = kRegPathVX;
  if (version == 3)
    kRegPath = kRegPathVXA;

  std::string value;
  auto result =
      ReadRegistryString(HKEY_LOCAL_MACHINE, kRegPath, key.c_str(), value);
  if (result) {
    return value;
  } else {
    return std::nullopt;
  }
}

}  // namespace win32
}  // namespace platform
