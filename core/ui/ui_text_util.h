// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <vector>
namespace urge::ui_text {
inline size_t Next(const std::string& text, size_t index) {
  index = std::min(index, text.size());
  if (index == text.size()) return index;
  ++index;
  while (index < text.size() &&
         (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80)
    ++index;
  return index;
}
inline size_t Previous(const std::string& text, size_t index) {
  index = std::min(index, text.size());
  if (!index) return 0;
  --index;
  while (index && (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80)
    --index;
  return index;
}
inline size_t Boundary(const std::string& text, size_t index) {
  index = std::min(index, text.size());
  while (index < text.size() && index &&
         (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80)
    --index;
  return index;
}
inline size_t Count(const std::string& text) {
  size_t result = 0;
  for (size_t i = 0; i < text.size(); i = Next(text, i)) ++result;
  return result;
}
inline std::string Limit(std::string text, size_t count) {
  size_t i = 0;
  for (size_t n = 0; n < count && i < text.size(); ++n) i = Next(text, i);
  text.resize(i);
  return text;
}
inline std::string Valid(const std::string& text) {
  std::string result;
  for (size_t i = 0; i < text.size();) {
    const unsigned char c = text[i];
    size_t n = c < 0x80                 ? 1
               : c >= 0xc2 && c <= 0xdf ? 2
               : c >= 0xe0 && c <= 0xef ? 3
               : c >= 0xf0 && c <= 0xf4 ? 4
                                        : 0;
    bool valid = n && i + n <= text.size();
    for (size_t k = 1; valid && k < n; ++k)
      valid = (static_cast<unsigned char>(text[i + k]) & 0xc0) == 0x80;
    if (valid && n >= 3) {
      const unsigned char second = text[i + 1];
      if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second >= 0xa0) ||
          (c == 0xf0 && second < 0x90) || (c == 0xf4 && second >= 0x90))
        valid = false;
    }
    if (valid) {
      if (c != 0) result.append(text, i, n);
      i += n;
    } else {
      result += "\xef\xbf\xbd";
      ++i;
    }
  }
  return result;
}
}  // namespace urge::ui_text
