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

#include "core/system/system_info.h"

#include <cstdint>
#include <ctime>
#include <iterator>
#include <string>

#include "SDL3/SDL_cpuinfo.h"
#include "SDL3/SDL_platform.h"

#if __has_include("urge_build_info.h")
#include "urge_build_info.h"
#else
#define URGE_GIT_COMMIT "unknown"
#endif

#if defined(SDL_PLATFORM_WIN32)
#include <windows.h>

#include "SDL3/SDL_stdinc.h"
#elif !defined(OS_EMSCRIPTEN)
#include <fstream>
#endif

namespace urge {

namespace system_info {

namespace {

#if defined(SDL_PLATFORM_WIN32)

std::string WideToUTF8(const wchar_t* text) {
  if (!text || !*text)
    return {};
  char* utf8 = SDL_iconv_wchar_utf8(text);
  if (!utf8)
    return {};
  std::string result(utf8);
  SDL_free(utf8);
  return result;
}

std::string ReadRegistryString(const wchar_t* path, const wchar_t* name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS)
    return {};

  wchar_t buffer[512] = {};
  DWORD size = sizeof(buffer);
  DWORD type = 0;
  const LONG status = RegQueryValueExW(key, name, nullptr, &type,
                                       reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);

  if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
    return {};
  buffer[std::size(buffer) - 1] = L'\0';
  return WideToUTF8(buffer);
}

DWORD ReadRegistryDword(const wchar_t* path, const wchar_t* name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS)
    return 0;

  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LONG status = RegQueryValueExW(key, name, nullptr, &type,
                                       reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);

  if (status != ERROR_SUCCESS || type != REG_DWORD)
    return 0;
  return value;
}

std::string WindowsProductName(ULONG major, ULONG minor, ULONG build) {
  if (major == 10)
    return build >= 22000 ? "Windows 11" : "Windows 10";
  if (major == 6 && minor == 3)
    return "Windows 8.1";
  if (major == 6 && minor == 2)
    return "Windows 8";
  if (major == 6 && minor == 1)
    return "Windows 7";
  return "Windows";
}

#endif  // SDL_PLATFORM_WIN32

[[maybe_unused]] std::string TrimText(std::string text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

}  // namespace

std::string git_commit() {
  return URGE_GIT_COMMIT;
}

std::string build_time() {
  static const std::string value = [] {
#if defined(SDL_PLATFORM_WIN32)
    // The PE header carries the link time, which is the one timestamp that
    // follows an incremental build without any help from the build system.
    const auto* module =
        reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (module) {
      const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
      if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        const auto* nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
        if (nt->Signature == IMAGE_NT_SIGNATURE) {
          const std::time_t stamp =
              static_cast<std::time_t>(nt->FileHeader.TimeDateStamp);
          std::tm local = {};
#if defined(_MSC_VER)
          localtime_s(&local, &stamp);
#else
          localtime_r(&stamp, &local);
#endif
          char buffer[32] = {};
          if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                            &local))
            return std::string(buffer);
        }
      }
    }
    return std::string();
#else
    return std::string(__DATE__ " " __TIME__);
#endif
  }();
  return value;
}

std::string os_version() {
  static const std::string value = [] {
#if defined(SDL_PLATFORM_WIN32)
    // RtlGetVersion reports the real build even when the executable has no
    // Windows 10+ manifest, unlike GetVersionEx.
    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
      auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
          GetProcAddress(ntdll, "RtlGetVersion"));
      if (rtl_get_version)
        rtl_get_version(&info);
    }

    const wchar_t* kCurrentVersion =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const std::string release =
        ReadRegistryString(kCurrentVersion, L"DisplayVersion");
    const DWORD ubr = ReadRegistryDword(kCurrentVersion, L"UBR");

    std::string result = WindowsProductName(
        info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
    if (!release.empty()) {
      result += " Version ";
      result += release;
    }
    if (info.dwBuildNumber != 0) {
      result += " (Build ";
      result += std::to_string(info.dwBuildNumber);
      if (ubr != 0) {
        result += '.';
        result += std::to_string(ubr);
      }
      result += ')';
    }
    return result;
#else
    return std::string(SDL_GetPlatform());
#endif
  }();
  return value;
}

std::string cpu_model() {
  static const std::string value = [] {
#if defined(SDL_PLATFORM_WIN32)
    const std::string brand = ReadRegistryString(
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");
    return brand.empty() ? std::string("Unknown") : TrimText(brand);
#elif !defined(OS_EMSCRIPTEN)
    // Linux and Android both describe the part in /proc/cpuinfo; take the
    // first line that names the model and leave the count to cpu_cores().
    std::ifstream file("/proc/cpuinfo");
    std::string line;
    while (std::getline(file, line)) {
      const bool named = line.rfind("model name", 0) == 0 ||
                         line.rfind("Hardware", 0) == 0 ||
                         line.rfind("Processor", 0) == 0;
      if (!named)
        continue;
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        const std::string model = TrimText(line.substr(colon + 1));
        if (!model.empty())
          return model;
      }
    }
    return std::string("Unknown");
#else
    return std::string("Unknown");
#endif
  }();
  return value;
}

int32_t cpu_cores() {
  static const int32_t value = SDL_GetNumLogicalCPUCores();
  return value;
}

uint64_t memory_mb() {
  static const uint64_t value = static_cast<uint64_t>(SDL_GetSystemRAM());
  return value;
}

}  // namespace system_info

}
