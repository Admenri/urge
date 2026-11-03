// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 URGE contributors.
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

#include "core/system/archive.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "physfs.h"

// RGSS resource containers expose only read-only PhysicsFS streams. No payload
// is unpacked or cached here: mounting reads names, sizes, offsets and keys.
namespace urge::rgss_archive {
namespace {

constexpr std::size_t kMaxName = 4096;
constexpr std::size_t kMaxNodes = 1 << 20;
constexpr std::size_t kMaxIndexNames = 128 * 1024 * 1024;

bool Fail(PHYSFS_ErrorCode error = PHYSFS_ERR_CORRUPT) {
  PHYSFS_setErrorCode(error);
  return false;
}

std::uint32_t NextKey(std::uint32_t key) {
  return key * 7u + 3u;
}

// Compose affine transformations modulo 2^32, so a seek costs O(log offset)
// instead of replaying the keystream from the beginning of the resource.
std::uint32_t AdvanceKey(std::uint32_t key, std::uint64_t words) {
  std::uint32_t multiplier = 7, increment = 3;
  std::uint32_t product = 1, sum = 0;
  while (words) {
    if (words & 1) {
      product *= multiplier;
      sum = sum * multiplier + increment;
    }
    increment *= multiplier + 1u;
    multiplier *= multiplier;
    words >>= 1;
  }
  return product * key + sum;
}

std::string FoldPath(std::string path) {
  for (char& ch : path) {
    if (ch == '\\')
      ch = '/';
    if (ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';
  }
  return path;
}

bool ValidName(const std::string& name) {
  if (name.empty() || name.front() == '/' || name.back() == '/')
    return false;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= name.size(); ++i) {
    if (i == name.size() || name[i] == '/') {
      const auto length = i - start;
      if (!length || (length == 1 && name[start] == '.') ||
          (length == 2 && name[start] == '.' && name[start + 1] == '.'))
        return false;
      start = i + 1;
    } else if (static_cast<unsigned char>(name[i]) < 32 || name[i] == ':') {
      return false;
    }
  }
  // Names in both container revisions are UTF-8, not the host's code page.
  for (std::size_t i = 0; i < name.size();) {
    auto ch = static_cast<unsigned char>(name[i++]);
    if (ch < 128)
      continue;
    int count;
    std::uint32_t value, minimum;
    if (ch >= 0xc2 && ch <= 0xdf) {
      count = 1;
      value = ch & 31;
      minimum = 0x80;
    } else if (ch >= 0xe0 && ch <= 0xef) {
      count = 2;
      value = ch & 15;
      minimum = 0x800;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
      count = 3;
      value = ch & 7;
      minimum = 0x10000;
    } else
      return false;
    if (name.size() - i < static_cast<std::size_t>(count))
      return false;
    while (count--) {
      ch = static_cast<unsigned char>(name[i++]);
      if ((ch & 0xc0) != 0x80)
        return false;
      value = (value << 6) | (ch & 63);
    }
    if (value < minimum || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff))
      return false;
  }
  return true;
}

struct Entry {
  std::uint64_t offset = 0, size = 0;
  std::uint32_t key = 0;
};

struct Node {
  bool directory = true;
  Entry entry;
  std::vector<std::string> children;
};

struct Archive {
  PHYSFS_Io* source;
  std::uint64_t length;
  std::size_t name_bytes = 0;
  std::unordered_map<std::string, Node> nodes;

  bool Read(void* buffer, std::uint64_t size, std::uint64_t limit) {
    const auto position = source->tell(source);
    if (position < 0 || static_cast<std::uint64_t>(position) > limit ||
        size > limit - static_cast<std::uint64_t>(position))
      return Fail();
    auto* out = static_cast<unsigned char*>(buffer);
    while (size) {
      const auto got = source->read(source, out, size);
      if (got < 0)
        return false;
      if (!got || static_cast<std::uint64_t>(got) > size)
        return Fail();
      size -= got;
      out += got;
    }
    return true;
  }

  bool Word(std::uint32_t& value, std::uint64_t limit) {
    unsigned char bytes[4];
    if (!Read(bytes, 4, limit))
      return false;
    value = std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
            (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    return true;
  }

  bool AddFile(std::string name, Entry entry) {
    std::replace(name.begin(), name.end(), '\\', '/');
    if (!ValidName(name))
      return Fail();
    std::string parent;
    std::size_t begin = 0;
    while (begin < name.size()) {
      const auto slash = name.find('/', begin);
      const bool last = slash == std::string::npos;
      const std::string part = name.substr(begin, last ? slash : slash - begin);
      const std::string key =
          parent.empty() ? FoldPath(part) : parent + '/' + FoldPath(part);
      auto found = nodes.find(key);
      if (found != nodes.end()) {
        if (last || !found->second.directory)
          return Fail();
      } else {
        const auto bytes = key.size() + part.size();
        if (nodes.size() >= kMaxNodes || bytes > kMaxIndexNames - name_bytes)
          return Fail(PHYSFS_ERR_OUT_OF_MEMORY);
        name_bytes += bytes;
        nodes[parent].children.push_back(part);
        Node node;
        node.directory = !last;
        node.entry = entry;
        nodes.emplace(key, std::move(node));
      }
      if (last)
        break;
      parent = key;
      begin = slash + 1;
    }
    return true;
  }

  bool ParseV1() {
    std::uint32_t key = 0xdeadcafeu;
    while (true) {
      const auto position = source->tell(source);
      if (position < 0)
        return false;
      if (static_cast<std::uint64_t>(position) == length)
        return true;
      std::uint32_t count, size;
      if (!Word(count, length))
        return false;
      count ^= key;
      key = NextKey(key);
      if (!count || count > kMaxName)
        return Fail();
      std::string name(count, '\0');
      if (!Read(name.data(), count, length))
        return false;
      for (char& ch : name) {
        ch = static_cast<char>(static_cast<unsigned char>(ch) ^ (key & 0xff));
        key = NextKey(key);
      }
      if (!Word(size, length))
        return false;
      size ^= key;
      key = NextKey(key);
      const auto offset = source->tell(source);
      if (offset < 0)
        return false;
      if (!AddFile(std::move(name),
                   {static_cast<std::uint64_t>(offset), size, key}))
        return false;
      // A truncated final payload does not invalidate preceding resources.
      // Its declared size remains indexed, but OpenRead will reject it. There
      // cannot be another reachable v1 header after a payload extending past
      // EOF.
      if (size > length - static_cast<std::uint64_t>(offset))
        return true;
      // The metadata key does not advance over the payload in revision 1.
      if (!source->seek(source, static_cast<std::uint64_t>(offset) + size))
        return false;
    }
  }

  bool ParseV3() {
    std::uint32_t seed;
    if (!Word(seed, length))
      return false;
    const std::uint32_t mask = seed * 9u + 3u;
    std::uint64_t data_start = length;
    while (true) {
      std::uint32_t offset, size, key, count;
      if (!Word(offset, data_start))
        return false;
      offset ^= mask;
      if (!offset)
        return true;
      data_start = std::min(data_start, static_cast<std::uint64_t>(offset));
      if (!Word(size, data_start) || !Word(key, data_start) ||
          !Word(count, data_start))
        return false;
      size ^= mask;
      key ^= mask;
      count ^= mask;
      if (!count || count > kMaxName)
        return Fail();
      std::string name(count, '\0');
      if (!Read(name.data(), count, data_start))
        return false;
      for (std::size_t i = 0; i < name.size(); ++i)
        name[i] = static_cast<char>(static_cast<unsigned char>(name[i]) ^
                                    ((mask >> ((i & 3) * 8)) & 0xff));
      if (!AddFile(std::move(name), {offset, size, key}))
        return false;
    }
  }
};

struct Stream {
  PHYSFS_Io io;
  PHYSFS_Io* source;
  Entry entry;
  std::uint64_t position = 0;
  std::uint32_t key;
};

PHYSFS_sint64 PHYSFS_CALL Read(PHYSFS_Io* io,
                               void* buffer,
                               PHYSFS_uint64 length) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  length = std::min<std::uint64_t>(
      {length, stream.entry.size - stream.position,
       static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())});
  if (!length)
    return 0;
  const auto got = stream.source->read(stream.source, buffer, length);
  if (got < 0)
    return -1;
  if (!got || static_cast<PHYSFS_uint64>(got) > length) {
    Fail();
    return -1;
  }
  auto* bytes = static_cast<unsigned char*>(buffer);
  auto remaining = static_cast<std::uint64_t>(got);
  // Preserve a partially consumed word across arbitrary read sizes and seeks.
  while (remaining && (stream.position & 3)) {
    *bytes++ ^= (stream.key >> ((stream.position & 3) * 8)) & 0xff;
    if (!(++stream.position & 3))
      stream.key = NextKey(stream.key);
    --remaining;
  }
  // Compute four consecutive states from the same base, breaking the serial
  // multiply dependency and allowing the compiler to batch the XOR portably.
  const auto little_endian = [](std::uint32_t value) {
    if constexpr (std::endian::native == std::endian::little) {
      return value;
    } else {
      return (value >> 24) | ((value >> 8) & 0x0000ff00u) |
             ((value << 8) & 0x00ff0000u) | (value << 24);
    }
  };
  while (remaining >= 16) {
    std::uint32_t words[4];
    std::memcpy(words, bytes, sizeof(words));
    words[0] ^= little_endian(stream.key);
    words[1] ^= little_endian(stream.key * 7u + 3u);
    words[2] ^= little_endian(stream.key * 49u + 24u);
    words[3] ^= little_endian(stream.key * 343u + 171u);
    std::memcpy(bytes, words, sizeof(words));
    stream.key = stream.key * 2401u + 1200u;
    bytes += 16;
    stream.position += 16;
    remaining -= 16;
  }
  while (remaining >= 4) {
    std::uint32_t word;
    std::memcpy(&word, bytes, 4);
    word ^= little_endian(stream.key);
    std::memcpy(bytes, &word, 4);
    stream.key = NextKey(stream.key);
    bytes += 4;
    stream.position += 4;
    remaining -= 4;
  }
  while (remaining) {
    *bytes++ ^= (stream.key >> ((stream.position & 3) * 8)) & 0xff;
    ++stream.position;
    --remaining;
  }
  return got;
}

PHYSFS_sint64 PHYSFS_CALL Write(PHYSFS_Io*, const void*, PHYSFS_uint64) {
  Fail(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int PHYSFS_CALL Seek(PHYSFS_Io* io, PHYSFS_uint64 offset) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  if (offset > stream.entry.size)
    return Fail(PHYSFS_ERR_PAST_EOF);
  if (!stream.source->seek(stream.source, stream.entry.offset + offset))
    return 0;
  stream.position = offset;
  stream.key = AdvanceKey(stream.entry.key, offset / 4);
  return 1;
}

PHYSFS_sint64 PHYSFS_CALL Tell(PHYSFS_Io* io) {
  return static_cast<Stream*>(io->opaque)->position;
}
PHYSFS_sint64 PHYSFS_CALL Length(PHYSFS_Io* io) {
  return static_cast<Stream*>(io->opaque)->entry.size;
}
int PHYSFS_CALL Flush(PHYSFS_Io*) {
  return 1;
}
void PHYSFS_CALL Destroy(PHYSFS_Io* io) {
  auto* stream = static_cast<Stream*>(io->opaque);
  stream->source->destroy(stream->source);
  delete stream;
}

PHYSFS_Io* MakeStream(PHYSFS_Io* source,
                      Entry entry,
                      std::uint64_t position = 0);
PHYSFS_Io* PHYSFS_CALL Duplicate(PHYSFS_Io* io) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  PHYSFS_Io* source = stream.source->duplicate(stream.source);
  return source ? MakeStream(source, stream.entry, stream.position) : nullptr;
}
PHYSFS_Io* MakeStream(PHYSFS_Io* source, Entry entry, std::uint64_t position) {
  auto* stream = new (std::nothrow) Stream{};
  if (!stream) {
    source->destroy(source);
    Fail(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }
  stream->source = source;
  stream->entry = entry;
  stream->io = {0,    stream, Read,      Write, Seek,
                Tell, Length, Duplicate, Flush, Destroy};
  if (!Seek(&stream->io, position)) {
    Destroy(&stream->io);
    return nullptr;
  }
  return &stream->io;
}

void* PHYSFS_CALL OpenArchive(PHYSFS_Io* io,
                              const char*,
                              int writing,
                              int* claimed) {
  if (writing) {
    Fail(PHYSFS_ERR_READ_ONLY);
    return nullptr;
  }
  const auto length = io->length(io);
  if (length < 0 || !io->seek(io, 0))
    return nullptr;
  auto archive = std::unique_ptr<Archive>(new (std::nothrow) Archive{
      io, static_cast<std::uint64_t>(length), 0, {}});
  if (!archive) {
    Fail(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }
  unsigned char header[8] = {};
  const auto count = std::min<std::uint64_t>(length, sizeof(header));
  if (!archive->Read(header, count, count))
    return nullptr;
  if (count < 6 || std::memcmp(header, "RGSSAD", 6)) {
    Fail(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }
  *claimed = 1;
  if (count != 8 || header[6] != 0) {
    Fail();
    return nullptr;
  }
  if (header[7] != 1 && header[7] != 3) {
    Fail(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }
  archive->nodes.emplace("", Node{});
  if (!(header[7] == 1 ? archive->ParseV1() : archive->ParseV3()))
    return nullptr;
  return archive.release();
}

PHYSFS_EnumerateCallbackResult PHYSFS_CALL
Enumerate(void* opaque,
          const char* dir,
          PHYSFS_EnumerateCallback callback,
          const char* original,
          void* data) {
  auto& nodes = static_cast<Archive*>(opaque)->nodes;
  const auto found = nodes.find(FoldPath(dir));
  if (found == nodes.end()) {
    Fail(PHYSFS_ERR_NOT_FOUND);
    return PHYSFS_ENUM_ERROR;
  }
  if (!found->second.directory) {
    Fail(PHYSFS_ERR_NOT_A_FILE);
    return PHYSFS_ENUM_ERROR;
  }
  for (const auto& name : found->second.children) {
    const auto result = callback(data, original, name.c_str());
    if (result != PHYSFS_ENUM_OK) {
      if (result == PHYSFS_ENUM_ERROR)
        Fail(PHYSFS_ERR_APP_CALLBACK);
      return result;
    }
  }
  return PHYSFS_ENUM_OK;
}

PHYSFS_Io* PHYSFS_CALL OpenRead(void* opaque, const char* name) {
  auto& archive = *static_cast<Archive*>(opaque);
  const auto found = archive.nodes.find(FoldPath(name));
  if (found == archive.nodes.end()) {
    Fail(PHYSFS_ERR_NOT_FOUND);
    return nullptr;
  }
  if (found->second.directory) {
    Fail(PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }
  const auto& entry = found->second.entry;
  if (entry.offset > archive.length ||
      entry.size > archive.length - entry.offset) {
    Fail(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }
  auto* source = archive.source->duplicate(archive.source);
  return source ? MakeStream(source, found->second.entry) : nullptr;
}

PHYSFS_Io* PHYSFS_CALL OpenWrite(void*, const char*) {
  Fail(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}
int PHYSFS_CALL ChangeDirectory(void*, const char*) {
  return Fail(PHYSFS_ERR_READ_ONLY);
}
int PHYSFS_CALL Stat(void* opaque, const char* name, PHYSFS_Stat* stat) {
  const auto& nodes = static_cast<Archive*>(opaque)->nodes;
  const auto found = nodes.find(FoldPath(name));
  if (found == nodes.end())
    return Fail(PHYSFS_ERR_NOT_FOUND);
  const auto& node = found->second;
  stat->filesize = node.directory ? 0 : node.entry.size;
  stat->modtime = stat->createtime = stat->accesstime = -1;
  stat->filetype =
      node.directory ? PHYSFS_FILETYPE_DIRECTORY : PHYSFS_FILETYPE_REGULAR;
  stat->readonly = 1;
  return 1;
}
void PHYSFS_CALL CloseArchive(void* opaque) {
  auto* archive = static_cast<Archive*>(opaque);
  archive->source->destroy(archive->source);
  delete archive;
}

const PHYSFS_Archiver kArchivers[] = {
    {0,
     {"RGSSAD", "RGSS read-only resource archive (v1)", "URGE", "", 0},
     OpenArchive,
     Enumerate,
     OpenRead,
     OpenWrite,
     OpenWrite,
     ChangeDirectory,
     ChangeDirectory,
     Stat,
     CloseArchive},
    {0,
     {"RGSS2A", "RGSS read-only resource archive (v1)", "URGE", "", 0},
     OpenArchive,
     Enumerate,
     OpenRead,
     OpenWrite,
     OpenWrite,
     ChangeDirectory,
     ChangeDirectory,
     Stat,
     CloseArchive},
    {0,
     {"RGSS3A", "RGSS read-only resource archive (v3)", "URGE", "", 0},
     OpenArchive,
     Enumerate,
     OpenRead,
     OpenWrite,
     OpenWrite,
     ChangeDirectory,
     ChangeDirectory,
     Stat,
     CloseArchive},
};
}  // namespace

bool Register() {
  for (const auto& archiver : kArchivers)
    if (!PHYSFS_registerArchiver(&archiver))
      return false;
  return true;
}

bool SupportsPath(const std::string& path) {
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos)
    return false;
  const auto extension = FoldPath(path.substr(dot));
  return extension == ".rgssad" || extension == ".rgss2a" ||
         extension == ".rgss3a";
}

bool Mount(const std::string& path, bool prepend) {
  if (!SupportsPath(path))
    return Fail(PHYSFS_ERR_UNSUPPORTED);
  return PHYSFS_mount(path.c_str(), nullptr, prepend ? 0 : 1) != 0;
}
}  // namespace urge::rgss_archive
