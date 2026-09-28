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

#include <string>
#include <vector>

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

}  // namespace

namespace platform {
namespace win32 {

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
