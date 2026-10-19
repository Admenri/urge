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
#include <cstdio>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace urge {

enum class LogLevel : std::uint8_t {
  kTrace = 0,
  kDebug,
  kInfo,
  kWarn,
  kError,

  kOff,
};

inline LogLevel GetLogLevel();

inline void SetLogLevel(LogLevel level);

inline void LogWrite(LogLevel level,
                     std::string_view prefix,
                     std::string_view message);

template <typename... Args>
void LogMessage(LogLevel level,
                std::string_view prefix,
                std::string_view format,
                Args&&... args) {
  if (level >= LogLevel::kOff || level < GetLogLevel())
    return;

  if constexpr (sizeof...(Args) == 0) {
    LogWrite(level, prefix, format);
  } else {
    LogWrite(level, prefix,
             std::vformat(format, std::make_format_args(args...)));
  }
}

namespace logger_detail {

#if defined(NDEBUG)
inline constexpr LogLevel kDefaultLevel = LogLevel::kInfo;
#else
inline constexpr LogLevel kDefaultLevel = LogLevel::kDebug;
#endif

inline LogLevel& CurrentLevel() {
  static LogLevel level = kDefaultLevel;
  return level;
}

}  // namespace logger_detail

inline LogLevel GetLogLevel() {
  return logger_detail::CurrentLevel();
}

inline void SetLogLevel(LogLevel level) {
  logger_detail::CurrentLevel() = level;
}

inline void LogWrite(LogLevel level,
                     std::string_view prefix,
                     std::string_view message) {
  static constexpr std::string_view kTags[] = {
      "[URGE] [TRACE] ", "[URGE] [DEBUG] ", "[URGE] [INFO] ",
      "[URGE] [WARN] ",  "[URGE] [ERROR] ",
  };
  static constexpr std::size_t kTagCount = std::size(kTags);

  const auto index = static_cast<std::size_t>(level);
  if (index >= kTagCount)
    return;

  const std::string_view tag = kTags[index];

  std::string line;
  std::size_t begin = 0;
  while (true) {
    const std::size_t end = message.find('\n', begin);
    const bool last = end == std::string_view::npos;

    line.assign(tag);
    line.append(prefix);
    line.append(message, begin, last ? std::string_view::npos : end - begin);
    line.push_back('\n');

    std::fwrite(line.data(), 1, line.size(), stderr);

    if (last)
      break;
    begin = end + 1;
  }

  std::fflush(stderr);
}

}  // namespace urge

#define URGE_LOGGER_STRINGIFY_IMPL(text) #text
#define URGE_LOGGER_STRINGIFY(text) URGE_LOGGER_STRINGIFY_IMPL(text)
#define URGE_LOGGER_LOCATION __FILE__ ":" URGE_LOGGER_STRINGIFY(__LINE__) ": "

#if defined(NDEBUG)

#define URGE_LOGGER_EMIT(level, ...)              \
  do {                                            \
    ::urge::LogMessage((level), "", __VA_ARGS__); \
  } while (false)

#define LOGGER_TRACE(...) ((void)0)
#define LOGGER_DEBUG(...) ((void)0)
#else

#define URGE_LOGGER_EMIT(level, ...)                                \
  do {                                                              \
    ::urge::LogMessage((level), URGE_LOGGER_LOCATION, __VA_ARGS__); \
  } while (false)
#define LOGGER_TRACE(...) \
  URGE_LOGGER_EMIT(::urge::LogLevel::kTrace, __VA_ARGS__)
#define LOGGER_DEBUG(...) \
  URGE_LOGGER_EMIT(::urge::LogLevel::kDebug, __VA_ARGS__)
#endif

#define LOGGER_INFO(...) URGE_LOGGER_EMIT(::urge::LogLevel::kInfo, __VA_ARGS__)
#define LOGGER_WARN(...) URGE_LOGGER_EMIT(::urge::LogLevel::kWarn, __VA_ARGS__)
#define LOGGER_ERROR(...) \
  URGE_LOGGER_EMIT(::urge::LogLevel::kError, __VA_ARGS__)
