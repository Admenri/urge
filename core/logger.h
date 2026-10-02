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

// The macros of this header are the whole API:
//
//   LOGGER_INFO("loaded {} entries", count);
//   LOGGER_ERROR("failed to open {}", path);
//
// The format string follows std::format, and a debug build behaves differently
// from a release build in two ways:
//
//   * LOGGER_TRACE and LOGGER_DEBUG disappear in a release build, so their
//     arguments are neither formatted nor evaluated, and nothing is emitted.
//   * The remaining messages carry a "file:line: " prefix in a debug build
//     only.
//
// A release build therefore reports the messages an issue report is written
// from, and a debug build adds the location of every one of them.
//
// Every message ends up on stderr, which the game has because it is built as a
// console application, and nothing else in the process is redirected.

namespace urge {

//! Severity of a message, ordered from the most to the least verbose.
enum class LogLevel : std::uint8_t {
  kTrace = 0,
  kDebug,
  kInfo,
  kWarn,
  kError,
  //! Discards every message, it is the highest level of the filter.
  kOff,
};

//! The level a message has to reach to be written, the default is kDebug in a
//! debug build and kInfo in a release build.
inline LogLevel GetLogLevel();
//! Lowers or raises the filter of GetLogLevel, it is not atomic and is meant
//! to be called from the main thread.
inline void SetLogLevel(LogLevel level);

//! Writes a message to the sink, the level tag and the newline are added here.
//! \param prefix a marker of the origin of the message, it is repeated on every
//! line, which matters because a message of a graphics backend is often a whole
//! block of text.
inline void LogWrite(LogLevel level,
                     std::string_view prefix,
                     std::string_view message);

//! Formats the arguments and forwards the result to LogWrite.
//! \param prefix an empty view for a message of the engine itself, or a marker
//! such as "[wgpu] " for a message that came from a library. A debug build
//! passes the "file:line: " location of the call here.
template <typename... Args>
void LogMessage(LogLevel level,
                std::string_view prefix,
                std::string_view format,
                Args&&... args) {
  if (level >= LogLevel::kOff || level < GetLogLevel())
    return;

  // NOTE: std::make_format_args only binds lvalue references, hence the named
  // parameters are used as lvalues. They stay alive until this call returns,
  // which covers the vformat() call below.
  LogWrite(level, prefix, std::vformat(format, std::make_format_args(args...)));
}

namespace logger_detail {

#if defined(NDEBUG)
inline constexpr LogLevel kDefaultLevel = LogLevel::kInfo;
#else
inline constexpr LogLevel kDefaultLevel = LogLevel::kDebug;
#endif

//! The state of the logger. The static local of an inline function is unique
//! across the translation units of a program, and its initialization is thread
//! safe, which is required because the callback of a graphics backend reports
//! its messages from whichever thread raised them.
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

  // A message of an unknown or disabled level is dropped.
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

    // A whole line is handed to a single write, the C runtime locks the stream
    // for the duration of the call, so the lines written by two threads cannot
    // end up spliced into each other.
    std::fwrite(line.data(), 1, line.size(), stderr);

    if (last)
      break;
    begin = end + 1;
  }

  std::fflush(stderr);
}

}  // namespace urge

//! Composes the "file:line: " prefix of a message at compile time.
#define URGE_LOGGER_STRINGIFY_IMPL(text) #text
#define URGE_LOGGER_STRINGIFY(text) URGE_LOGGER_STRINGIFY_IMPL(text)
#define URGE_LOGGER_LOCATION __FILE__ ":" URGE_LOGGER_STRINGIFY(__LINE__) ": "

#if defined(NDEBUG)
//! A release build drops the location, it is of no use to the player.
#define URGE_LOGGER_EMIT(level, ...)              \
  do {                                            \
    ::urge::LogMessage((level), "", __VA_ARGS__); \
  } while (false)
//! A release build has no trace and no debug message at all, so the arguments
//! of the two spellings below are not even evaluated.
#define LOGGER_TRACE(...) ((void)0)
#define LOGGER_DEBUG(...) ((void)0)
#else
//! A debug build keeps the location of every message.
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
