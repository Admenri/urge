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

#include "core/audio/decoders/asf.h"
#include <algorithm>
#include <array>
#include <map>

namespace urge::audioformat {
namespace {
constexpr uint8_t kHeader[] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                               0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
constexpr uint8_t kData[] = {0x36, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                             0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
constexpr uint8_t kFile[] = {0xa1, 0xdc, 0xab, 0x8c, 0x47, 0xa9, 0xcf, 0x11,
                             0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
constexpr uint8_t kStream[] = {0x91, 0x07, 0xdc, 0xb7, 0xb7, 0xa9, 0xcf, 0x11,
                               0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
constexpr uint8_t kAudio[] = {0x40, 0x9e, 0x69, 0xf8, 0x4d, 0x5b, 0xcf, 0x11,
                              0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b};
bool GUID(Bytes bytes, size_t offset, const uint8_t* guid) {
  return Range(bytes, offset, 16) &&
         std::memcmp(bytes.data() + offset, guid, 16) == 0;
}
struct PacketReader {
  Bytes bytes;
  size_t pos = 0;
  bool valid = true;
  uint32_t Field(unsigned code) {
    const unsigned sizes[] = {0, 1, 2, 4};
    unsigned count = sizes[code & 3];
    if (!Range(bytes, pos, count)) {
      valid = false;
      return 0;
    }
    uint32_t value = count == 4   ? LE32(bytes, pos)
                     : count == 2 ? LE16(bytes, pos)
                     : count      ? bytes[pos]
                                  : 0;
    pos += count;
    return value;
  }
  Bytes Take(size_t size) {
    if (!Range(bytes, pos, size)) {
      valid = false;
      return {};
    }
    auto out = bytes.subspan(pos, size);
    pos += size;
    return out;
  }
};
struct Assembly {
  uint32_t total = 0;
  std::vector<uint8_t> data;
};
bool Packet(Bytes packet,
            uint8_t stream_id,
            WMAStream& out,
            std::map<uint32_t, Assembly>& pending) {
  PacketReader r{packet};
  if (packet.empty())
    return false;
  if (packet[0] & 128) {
    uint8_t ec = static_cast<uint8_t>(r.Field(1));
    if (ec & 0x70)
      return false;
    r.Take(ec & 15);
  }
  uint8_t flags = static_cast<uint8_t>(r.Field(1));
  uint8_t props = static_cast<uint8_t>(r.Field(1));
  uint32_t length = r.Field(flags >> 5), sequence = r.Field(flags >> 1),
           padding = r.Field(flags >> 3);
  (void)sequence;
  if (length && length != packet.size())
    return false;
  r.Take(6);  // send time and duration
  unsigned count = 1, length_type = 0;
  if (flags & 1) {
    uint8_t pf = static_cast<uint8_t>(r.Field(1));
    count = pf & 63;
    length_type = pf >> 6;
  }
  if (!count || padding > packet.size() || r.pos > packet.size() - padding)
    return false;
  size_t end = packet.size() - padding;
  for (unsigned i = 0; i < count; ++i) {
    uint32_t id = r.Field(props >> 6) & 127;
    uint32_t object = r.Field(props >> 4), offset = r.Field(props >> 2),
             repl = r.Field(props);
    Bytes extra = r.Take(repl);
    uint32_t size =
        (flags & 1) ? r.Field(length_type)
                    : (r.pos <= end ? static_cast<uint32_t>(end - r.pos) : 0);
    if (!r.valid || r.pos > end || size > end - r.pos)
      return false;
    Bytes payload = r.Take(size);
    if (id != stream_id)
      continue;
    if (repl == 1) {
      // Compressed payload: one-byte length prefixes for complete objects.
      size_t p = 0;
      while (p < payload.size()) {
        size_t n = payload[p++];
        if (!n || !Range(payload, p, n) || n != out.align)
          return false;
        out.packets.emplace_back(payload.begin() + p, payload.begin() + p + n);
        p += n;
      }
    } else {
      if (repl && repl < 8)
        return false;
      uint32_t total = repl >= 8 ? LE32(extra, 0) : out.align;
      if (total != out.align || offset > total ||
          payload.size() > total - offset)
        return false;
      auto& a = pending[object];
      if (!offset) {
        a.total = total;
        a.data.clear();
        a.data.reserve(total);
      }
      if (a.total != total || a.data.size() != offset)
        return false;
      a.data.insert(a.data.end(), payload.begin(), payload.end());
      if (a.data.size() == a.total) {
        out.packets.push_back(std::move(a.data));
        pending.erase(object);
      }
      if (pending.size() > 32)
        return false;
    }
    if (out.packets.size() > 1000000)
      return false;
  }
  return r.valid && r.pos <= end;
}
}  // namespace
bool IsWMA(Bytes bytes) {
  return GUID(bytes, 0, kHeader);
}
bool ReadASF(Bytes bytes, WMAStream& out, std::string& error) {
  auto fail = [&](const char* message) {
    error = message;
    return false;
  };
  if (!IsWMA(bytes) || bytes.size() < 30 || LE64(bytes, 16) < 30 ||
      LE64(bytes, 16) > bytes.size() || bytes.size() > 256 * 1024 * 1024)
    return fail("invalid ASF header or file exceeds 256 MiB");
  size_t header_end = static_cast<size_t>(LE64(bytes, 16)), pos = 30;
  uint32_t packet_size = 0;
  uint8_t stream_id = 0;
  uint32_t count = LE32(bytes, 24);
  if (count > 65536)
    return fail("too many ASF header objects");
  for (uint32_t i = 0; i < count; ++i) {
    if (!Range(bytes, pos, 24))
      return fail("truncated ASF header object");
    uint64_t size = LE64(bytes, pos + 16);
    if (size < 24 || size > header_end - pos)
      return fail("invalid ASF object size");
    Bytes data = bytes.subspan(pos + 24, static_cast<size_t>(size) - 24);
    if (GUID(bytes, pos, kFile)) {
      if (data.size() < 80 || LE32(data, 68) != LE32(data, 72) ||
          LE32(data, 72) > 16 * 1024 * 1024)
        return fail("unsupported variable-size ASF packets");
      packet_size = LE32(data, 72);
    } else if (GUID(bytes, pos, kStream) && GUID(data, 0, kAudio) &&
               !stream_id) {
      if (data.size() < 72 || LE16(data, 48) & 0x8000 || LE32(data, 40) < 18 ||
          !Range(data, 54, LE32(data, 40)))
        return fail("invalid or encrypted ASF audio stream");
      Bytes fmt = data.subspan(54, LE32(data, 40));
      uint16_t tag = LE16(fmt, 0);
      if (tag != 0x160 && tag != 0x161)
        return fail("WMA Pro/Lossless/DRM are not supported");
      out.version = tag == 0x160 ? 1 : 2;
      out.channels = LE16(fmt, 2);
      out.rate = LE32(fmt, 4);
      out.bytes_per_second = LE32(fmt, 8);
      out.align = LE16(fmt, 12);
      uint16_t extra = LE16(fmt, 16);
      if (!Range(fmt, 18, extra) || extra < (out.version == 1 ? 4 : 6))
        return fail("truncated WMA codec configuration");
      out.flags = LE16(fmt, out.version == 1 ? 20 : 22);
      stream_id = LE16(data, 48) & 127;
    }
    pos += static_cast<size_t>(size);
  }
  if (!stream_id || !packet_size || !out.align || out.rate < 1000 ||
      out.rate > 48000 || !out.channels || out.channels > 2 ||
      !out.bytes_per_second || out.bytes_per_second > 1000000)
    return fail("unsupported WMA stream parameters");
  pos = header_end;
  std::map<uint32_t, Assembly> pending;
  while (Range(bytes, pos, 24)) {
    uint64_t size = LE64(bytes, pos + 16);
    if (size < 24 || size > bytes.size() - pos)
      return fail("truncated ASF data object");
    if (GUID(bytes, pos, kData)) {
      if (size < 50)
        return fail("truncated ASF data header");
      size_t begin = pos + 50, end = pos + static_cast<size_t>(size);
      while (begin < end) {
        if (packet_size > end - begin ||
            !Packet(bytes.subspan(begin, packet_size), stream_id, out, pending))
          return fail("invalid ASF audio payload");
        begin += packet_size;
      }
      break;
    }
    pos += static_cast<size_t>(size);
  }
  if (out.packets.empty() || !pending.empty())
    return fail("incomplete ASF audio objects");
  return true;
}
}  // namespace urge::audioformat
