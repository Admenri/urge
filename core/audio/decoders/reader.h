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

#include <cstdint>
#include <cstring>
#include <span>
#include <string>

namespace urge::audioformat {
using Bytes = std::span<const uint8_t>;
inline bool Range(Bytes bytes, size_t offset, size_t count) {
  return offset <= bytes.size() && count <= bytes.size() - offset;
}
inline bool Tag(Bytes bytes, size_t offset, const char* tag) {
  return Range(bytes, offset, 4) &&
         std::memcmp(bytes.data() + offset, tag, 4) == 0;
}
inline uint16_t LE16(Bytes b, size_t p) {
  return Range(b, p, 2) ? b[p] | (uint16_t(b[p + 1]) << 8) : 0;
}
inline uint32_t LE32(Bytes b, size_t p) {
  return Range(b, p, 4) ? LE16(b, p) | (uint32_t(LE16(b, p + 2)) << 16) : 0;
}
inline uint64_t LE64(Bytes b, size_t p) {
  return LE32(b, p) | (uint64_t(LE32(b, p + 4)) << 32);
}
inline uint16_t BE16(Bytes b, size_t p) {
  return Range(b, p, 2) ? (uint16_t(b[p]) << 8) | b[p + 1] : 0;
}
inline uint32_t BE32(Bytes b, size_t p) {
  return (uint32_t(BE16(b, p)) << 16) | BE16(b, p + 2);
}
// RIFF children are bounded by their parent, including odd-byte padding.
struct Chunk {
  Bytes bytes;
  Bytes data;
  bool Is(const char* id) const { return Tag(bytes, 0, id); }
  bool List(const char* type) const {
    return (Is("LIST") || Is("RIFF")) && Tag(data, 0, type);
  }
};
class Chunks {
 public:
  explicit Chunks(Bytes bytes) : remaining_(bytes) {}
  bool Next(Chunk& out) {
    if (remaining_.empty())
      return false;
    if (remaining_.size() < 8) {
      valid_ = false;
      return false;
    }
    const size_t n = LE32(remaining_, 4);
    if (!Range(remaining_, 8, n) || !Range(remaining_, 8 + n, n & 1)) {
      valid_ = false;
      return false;
    }
    out = {remaining_.first(8 + n), remaining_.subspan(8, n)};
    remaining_ = remaining_.subspan(8 + n + (n & 1));
    return true;
  }
  bool valid() const { return valid_; }

 private:
  Bytes remaining_;
  bool valid_ = true;
};
inline bool RIFF(Bytes bytes, const char* type, Bytes& body) {
  if (!Tag(bytes, 0, "RIFF") || !Tag(bytes, 8, type) || LE32(bytes, 4) < 4 ||
      !Range(bytes, 8, LE32(bytes, 4)))
    return false;
  body = bytes.subspan(12, LE32(bytes, 4) - 4);
  return true;
}
}  // namespace urge::audioformat
