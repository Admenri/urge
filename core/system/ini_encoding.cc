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

#include "core/system/ini_encoding.h"

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "SDL3/SDL_stdinc.h"
#include "core/common/inirw.h"

#if defined(SDL_PLATFORM_WIN32)
#include <windows.h>
#elif defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#include "SDL3/SDL_system.h"
#elif defined(SDL_PLATFORM_EMSCRIPTEN)
#include <emscripten.h>
#elif defined(URGE_USE_SYSTEM_ICONV)
#include <iconv.h>
#include <cerrno>
#endif

namespace urge::ini_encoding {

bool IsUTF8(std::string_view text) {
  size_t i = 0;
  while (i < text.size()) {
    const auto c = static_cast<unsigned char>(text[i++]);
    if (c == 0)
      return false;
    if (c < 0x80)
      continue;
    const size_t count = c >= 0xC2 && c <= 0xDF   ? 1
                         : c >= 0xE0 && c <= 0xEF ? 2
                         : c >= 0xF0 && c <= 0xF4 ? 3
                                                  : 0;
    if (!count || count > text.size() - i)
      return false;
    const auto first = static_cast<unsigned char>(text[i]);
    if ((c == 0xE0 && first < 0xA0) || (c == 0xED && first >= 0xA0) ||
        (c == 0xF0 && first < 0x90) || (c == 0xF4 && first >= 0x90))
      return false;
    for (size_t j = 0; j < count; ++j)
      if ((static_cast<unsigned char>(text[i++]) & 0xC0) != 0x80)
        return false;
  }
  return true;
}

namespace {

#if defined(SDL_PLATFORM_ANDROID)
bool AndroidConvert(std::string_view text,
                    const std::string& from,
                    const std::string& to,
                    std::string& output) {
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  if (!env || text.size() > INT_MAX)
    return false;
  if (env->PushLocalFrame(24) != 0) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return false;
  }
  const auto failed = [&]() {
    const bool pending = env->ExceptionCheck();
    if (pending)
      env->ExceptionClear();
    return pending;
  };
  // String's codec replaces invalid/unrepresentable characters. Verify both
  // round trips so those replacements are treated as a conversion failure.
  const auto convert = [&]() -> bool {
    jclass cls = env->FindClass("java/lang/String");
    if (failed() || !cls)
      return false;
    jmethodID constructor =
        env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
    if (failed() || !constructor)
      return false;
    jmethodID get_bytes =
        env->GetMethodID(cls, "getBytes", "(Ljava/lang/String;)[B");
    if (failed() || !get_bytes)
      return false;
    jmethodID equals = env->GetMethodID(cls, "equals", "(Ljava/lang/Object;)Z");
    if (failed() || !equals)
      return false;
    jstring source = env->NewStringUTF(from.c_str());
    if (failed() || !source)
      return false;
    jstring destination = env->NewStringUTF(to.c_str());
    if (failed() || !destination)
      return false;
    jbyteArray input = env->NewByteArray(static_cast<jsize>(text.size()));
    if (failed() || !input)
      return false;
    env->SetByteArrayRegion(input, 0, static_cast<jsize>(text.size()),
                            reinterpret_cast<const jbyte*>(text.data()));
    if (failed())
      return false;
    jobject value = env->NewObject(cls, constructor, input, source);
    if (failed() || !value)
      return false;
    const auto bytes = [&](jobject string, jstring charset,
                           std::string& result) {
      auto array = static_cast<jbyteArray>(
          env->CallObjectMethod(string, get_bytes, charset));
      if (failed() || !array)
        return false;
      const jsize length = env->GetArrayLength(array);
      result.resize(static_cast<size_t>(length));
      env->GetByteArrayRegion(array, 0, length,
                              reinterpret_cast<jbyte*>(result.data()));
      return !failed();
    };
    std::string original;
    if (!bytes(value, source, original) || original != text)
      return false;
    if (!bytes(value, destination, output))
      return false;
    jbyteArray encoded = env->NewByteArray(static_cast<jsize>(output.size()));
    if (failed() || !encoded)
      return false;
    env->SetByteArrayRegion(encoded, 0, static_cast<jsize>(output.size()),
                            reinterpret_cast<const jbyte*>(output.data()));
    if (failed())
      return false;
    jobject roundtrip = env->NewObject(cls, constructor, encoded, destination);
    if (failed() || !roundtrip)
      return false;
    const bool same = env->CallBooleanMethod(value, equals, roundtrip);
    return !failed() && same;
  };
  const bool success = convert();
  env->PopLocalFrame(nullptr);
  if (!success)
    output.clear();
  return success;
}
#endif

#if defined(SDL_PLATFORM_EMSCRIPTEN)
// clang-format off
EM_JS(char*, WebConvert,
      (const char* data, size_t length, const char* from, const char* to, size_t* size), {
  try {
    const alias = (name) => {
      name = name.toLowerCase().replaceAll('_', '-');
      const aliases = {'cp932':'shift-jis', 'cp936':'gbk', 'cp950':'big5',
                       'cp949':'euc-kr', 'utf8':'utf-8'};
      return aliases[name] || name.replace(/^cp(125[0-8])$/, 'windows-$1');
    };
    const source = alias(UTF8ToString(from));
    const destination = alias(UTF8ToString(to));
    let result;
    if (destination === 'utf-8') {
      const decoder = new TextDecoder(source, {fatal:true, ignoreBOM:true});
      result = new TextEncoder().encode(decoder.decode(HEAPU8.subarray(data, data + length)));
    } else {
      // Browsers only expose a UTF-8 encoder. Legacy INIs can still save ASCII
      // settings by patching their original bytes, preserving other text.
      const charset = new TextDecoder(destination, {fatal:true}).encoding;
      if (source !== 'utf-8' || charset.startsWith('utf-16') || charset === 'iso-2022-jp')
        return 0;
      result = HEAPU8.slice(data, data + length);
      if (result.some((value) => value === 0 || value >= 128))
        return 0;
    }
    const pointer = _malloc(Math.max(1, result.length));
    if (!pointer)
      return 0;
    HEAPU8.set(result, pointer);
    HEAPU32[size >> 2] = result.length;
    return pointer;
  } catch (error) {
    return 0;
  }
});
// clang-format on
#endif

bool ValidEncodingName(const std::string& name) {
  if (name.empty() || name.size() > 64)
    return false;
  for (unsigned char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
      return false;
  return true;
}

#if defined(SDL_PLATFORM_WIN32)
bool CodePage(const std::string& name, UINT& page) {
  const struct {
    const char* name;
    UINT page;
  } aliases[] = {{"UTF-8", CP_UTF8}, {"UTF8", CP_UTF8}, {"GB18030", 54936},
                 {"GBK", 936},       {"GB2312", 936},   {"SHIFT_JIS", 932},
                 {"SHIFT-JIS", 932}, {"SJIS", 932},     {"BIG5", 950},
                 {"EUC-KR", 51949},  {"ASCII", 20127},  {"ISO-8859-1", 28591}};
  for (const auto& alias : aliases)
    if (SDL_strcasecmp(name.c_str(), alias.name) == 0) {
      page = alias.page;
      return true;
    }
  const char* digits = name.c_str();
  if (SDL_strncasecmp(digits, "CP", 2) == 0)
    digits += 2;
  else if (SDL_strncasecmp(digits, "WINDOWS-", 8) == 0)
    digits += 8;
  else
    return false;
  if (!*digits)
    return false;
  page = 0;
  for (; *digits; ++digits) {
    if (*digits < '0' || *digits > '9' || page > 6553)
      return false;
    page = page * 10 + (*digits - '0');
  }
  return IsValidCodePage(page) != 0;
}
#endif

}  // namespace

bool Convert(std::string_view text,
             const std::string& from,
             const std::string& to,
             std::string& output) {
  output.clear();
  if (!ValidEncodingName(from) || !ValidEncodingName(to))
    return false;
#if defined(URGE_USE_SYSTEM_ICONV)
  iconv_t converter = iconv_open(to.c_str(), from.c_str());
  if (converter == reinterpret_cast<iconv_t>(-1))
    return false;
  char* input = const_cast<char*>(text.data());
  size_t left = text.size();
  bool flushing = false;
  bool success = false;
  for (;;) {
    char buffer[4096];
    char* cursor = buffer;
    size_t available = sizeof(buffer);
    const size_t result =
        iconv(converter, flushing ? nullptr : &input,
              flushing ? nullptr : &left, &cursor, &available);
    output.append(buffer, sizeof(buffer) - available);
    if (result == static_cast<size_t>(-1)) {
      if (errno == E2BIG)
        continue;
      break;
    }
    // Non-reversible substitutions are not suitable for configuration paths.
    if (result != 0)
      break;
    if (flushing) {
      success = true;
      break;
    }
    flushing = true;
  }
  iconv_close(converter);
  if (!success)
    output.clear();
  return success;
#else
  // SDL's built-in converter covers Unicode on platforms without iconv.
  const auto unicode = [](const std::string& name) {
    return name == "UTF-8" || name == "UTF-16LE" || name == "UTF-16BE" ||
           name == "UTF-32LE" || name == "UTF-32BE";
  };
  if (unicode(from) && unicode(to)) {
    // SDL's fallback may replace out-of-range UTF-32 scalars. Validate first
    // so malformed Unicode follows the same ASCII-filtering policy everywhere.
    if (from == "UTF-8") {
      if (!IsUTF8(text))
        return false;
    } else {
      const size_t width = from.find("32") != std::string::npos ? 4 : 2;
      if (text.size() % width)
        return false;
      const bool big_endian = from.ends_with("BE");
      const auto unit = [&](size_t offset) {
        uint32_t value = 0;
        for (size_t j = 0; j < width; ++j)
          value = (value << 8) |
                  static_cast<unsigned char>(
                      text[offset + (big_endian ? j : width - j - 1)]);
        return value;
      };
      for (size_t i = 0; i < text.size(); i += width) {
        const uint32_t value = unit(i);
        if (value == 0 || value > 0x10FFFF)
          return false;
        if (value >= 0xD800 && value <= 0xDFFF) {
          if (width != 2 || value >= 0xDC00 || i + width == text.size())
            return false;
          i += width;
          const uint32_t low = unit(i);
          if (low < 0xDC00 || low > 0xDFFF)
            return false;
        }
      }
    }
    SDL_iconv_t converter = SDL_iconv_open(to.c_str(), from.c_str());
    if (converter == reinterpret_cast<SDL_iconv_t>(SDL_ICONV_ERROR))
      return false;
    const char* input = text.data();
    size_t left = text.size();
    bool success = true;
    while (left) {
      char buffer[4096];
      char* cursor = buffer;
      size_t available = sizeof(buffer);
      const size_t result =
          SDL_iconv(converter, &input, &left, &cursor, &available);
      output.append(buffer, sizeof(buffer) - available);
      if (result == SDL_ICONV_ERROR || result == SDL_ICONV_EILSEQ ||
          result == SDL_ICONV_EINVAL) {
        success = false;
        break;
      }
    }
    SDL_iconv_close(converter);
    if (!success)
      output.clear();
    return success;
  }
#if defined(SDL_PLATFORM_ANDROID)
  return AndroidConvert(text, from, to, output);
#elif defined(SDL_PLATFORM_EMSCRIPTEN)
  size_t size = 0;
  char* converted =
      WebConvert(text.data(), text.size(), from.c_str(), to.c_str(), &size);
  if (!converted)
    return false;
  output.assign(converted, size);
  std::free(converted);
  return true;
#elif defined(SDL_PLATFORM_WIN32)
  UINT source, destination;
  if (!CodePage(from, source) || !CodePage(to, destination) ||
      text.size() > INT_MAX)
    return false;
  if (text.empty())
    return true;
  const int length =
      MultiByteToWideChar(source, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0)
    return false;
  std::wstring wide(static_cast<size_t>(length), L'\0');
  if (MultiByteToWideChar(source, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), wide.data(),
                          length) != length)
    return false;
  const bool unicode_page = destination == CP_UTF8 || destination == 54936;
  const DWORD flags =
      unicode_page ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
  BOOL substituted = FALSE;
  BOOL* used_default = unicode_page ? nullptr : &substituted;
  const int size = WideCharToMultiByte(destination, flags, wide.data(), length,
                                       nullptr, 0, nullptr, used_default);
  if (size <= 0 || substituted)
    return false;
  output.resize(static_cast<size_t>(size));
  if (WideCharToMultiByte(destination, flags, wide.data(), length,
                          output.data(), size, nullptr, used_default) != size ||
      substituted) {
    output.clear();
    return false;
  }
  return true;
#else
  return false;
#endif
#endif
}

bool Decode(const std::string& bytes,
            std::string& text,
            std::string& encoding,
            std::string& bom,
            std::string& error) {
  encoding = "UTF-8";
  bom.clear();
  error.clear();
  const struct {
    const char* bytes;
    size_t size;
    const char* encoding;
  } markers[] = {{"\xFF\xFE\0\0", 4, "UTF-32LE"},
                 {"\0\0\xFE\xFF", 4, "UTF-32BE"},
                 {"\xEF\xBB\xBF", 3, "UTF-8"},
                 {"\xFF\xFE", 2, "UTF-16LE"},
                 {"\xFE\xFF", 2, "UTF-16BE"}};
  for (const auto& marker : markers)
    if (bytes.compare(0, marker.size, marker.bytes, marker.size) == 0) {
      encoding = marker.encoding;
      bom.assign(marker.bytes, marker.size);
      break;
    }
  const std::string_view payload(bytes.data() + bom.size(),
                                 bytes.size() - bom.size());
  if (bom.empty()) {
    ini::IniFile raw(bytes, true);
    const char* override = SDL_getenv("URGE_INI_ENCODING");
    const std::string requested =
        override && *override ? override : raw.Get("Game", "IniEncoding");
    if (!requested.empty()) {
      encoding = requested;
    } else if (!IsUTF8(payload)) {
#if defined(SDL_PLATFORM_WIN32)
      encoding = "CP" + std::to_string(GetACP());
#else
      encoding = "GB18030";
#endif
    }
  }
  bool converted = true;
  if (!ValidEncodingName(encoding)) {
    error =
        "Invalid Game.ini encoding name. Use an ASCII code-page name "
        "such as GB18030 or CP932.";
    encoding = "unknown";
    converted = false;
  } else if (encoding == "UTF-8") {
    text.assign(payload);
  } else if (!Convert(payload, encoding, "UTF-8", text)) {
    error =
        "Cannot decode Game.ini using " + encoding +
        ". Convert it to UTF-8, or specify [Game] IniEncoding / "
        "URGE_INI_ENCODING "
        "(for example GB18030, CP932 or BIG5). The encoding must be supported "
        "by this platform.";
    converted = false;
  }
  if (converted && IsUTF8(text))
    return true;
  if (error.empty())
    error = "Invalid Unicode or embedded NUL in Game.ini.";

  // Do not feed invalid text to a platform window API. With a Unicode BOM,
  // inspect code units: taking raw low bytes could invent ASCII INI syntax.
  text.clear();
  const bool wide = bom.size() && encoding != "UTF-8";
  const size_t width = !wide                                      ? 1
                       : encoding.find("32") != std::string::npos ? 4
                                                                  : 2;
  const bool big_endian = wide && encoding.ends_with("BE");
  for (size_t i = 0; width <= payload.size() - i; i += width) {
    uint32_t value = 0;
    for (size_t j = 0; j < width; ++j) {
      const size_t offset = big_endian ? j : width - j - 1;
      value = (value << 8) | static_cast<unsigned char>(payload[i + offset]);
    }
    if (value > 0 && value < 0x80)
      text.push_back(static_cast<char>(value));
  }
  error +=
      " Non-ASCII characters were filtered; the original INI will not be "
      "overwritten.";
  return false;
}

}  // namespace urge::ini_encoding
