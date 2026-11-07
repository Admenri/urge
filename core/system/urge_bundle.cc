// Licensed under the MIT License. SPDX-License-Identifier: MIT
// Copyright (c) 2026 URGE contributors.

#include "core/system/urge_bundle.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "core/system/urge_bundle_format.h"
#include "physfs.h"

namespace urge::urge_bundle {
namespace {
using namespace format;

struct Password {
  std::mutex mutex;
  std::string text;
  ~Password() { crypto_wipe(text.data(), text.size()); }
} g_password;

bool Fail(PHYSFS_ErrorCode error = PHYSFS_ERR_CORRUPT) {
  PHYSFS_setErrorCode(error);
  return false;
}

bool ReadExact(PHYSFS_Io* source, void* buffer, std::uint64_t size) {
  auto* out = static_cast<std::uint8_t*>(buffer);
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

struct Entry {
  std::uint64_t offset = 0, size = 0;
  std::array<std::uint8_t, kNonceSize> nonce{};
};
struct Node {
  bool directory = true;
  Entry entry;
  std::vector<std::string> children;
};
struct Archive {
  PHYSFS_Io* source = nullptr;
  Key key;
  std::unordered_map<std::string, Node> nodes;
  std::size_t name_bytes = 0;

  bool AddFile(const std::string& name, const Entry& entry) {
    if (!ValidName(name))
      return Fail();
    std::string parent;
    std::size_t begin = 0;
    while (begin < name.size()) {
      const auto slash = name.find('/', begin);
      const bool last = slash == std::string::npos;
      const auto part = name.substr(begin, last ? slash : slash - begin);
      const auto path =
          parent.empty() ? FoldPath(part) : parent + '/' + FoldPath(part);
      const auto found = nodes.find(path);
      if (found != nodes.end()) {
        if (last || !found->second.directory)
          return Fail();
      } else {
        const auto bytes = path.size() + part.size();
        if (nodes.size() >= kMaxNodes || bytes > kMaxIndexSize - name_bytes)
          return Fail(PHYSFS_ERR_OUT_OF_MEMORY);
        name_bytes += bytes;
        nodes[parent].children.push_back(part);
        Node node;
        node.directory = !last;
        node.entry = entry;
        nodes.emplace(path, std::move(node));
      }
      if (last)
        break;
      parent = path;
      begin = slash + 1;
    }
    return true;
  }

  bool Parse(const std::vector<std::uint8_t>& index,
             std::uint64_t data_start,
             std::uint64_t length) {
    const auto count = Get(index.data(), 4);
    if (count >= kMaxNodes || count > (index.size() - 4) / 45)
      return Fail();
    nodes.emplace("", Node{});
    std::size_t position = 4;
    std::uint64_t offset = data_start;
    for (std::uint64_t i = 0; i < count; ++i) {
      if (index.size() - position < 4)
        return Fail();
      const auto name_size = Get(index.data() + position, 4);
      position += 4;
      if (!name_size || name_size > kMaxName ||
          name_size > index.size() - position ||
          index.size() - position - name_size < 40)
        return Fail();
      std::string name(reinterpret_cast<const char*>(index.data() + position),
                       static_cast<std::size_t>(name_size));
      position += static_cast<std::size_t>(name_size);
      Entry entry;
      const auto relative = Get(index.data() + position, 8);
      entry.size = Get(index.data() + position + 8, 8);
      std::memcpy(entry.nonce.data(), index.data() + position + 16, kNonceSize);
      position += 40;
      // Canonical contiguous layout rejects overlaps, gaps and integer wraps.
      if (relative != offset - data_start || entry.size > length - offset)
        return Fail();
      const auto tags = ChunkCount(entry.size) * kTagSize;
      if (tags > length - offset - entry.size)
        return Fail();
      entry.offset = offset;
      offset += entry.size + tags;
      if (!AddFile(name, entry))
        return false;
    }
    return (position == index.size() && offset == length) || Fail();
  }
};

struct Stream {
  PHYSFS_Io io{};
  PHYSFS_Io* source = nullptr;
  Entry entry;
  Key key;
  std::uint64_t position = 0;
  std::uint64_t cached_chunk = std::numeric_limits<std::uint64_t>::max();
  std::array<std::uint8_t, kChunkSize> plain{};
  ~Stream() { crypto_wipe(plain.data(), plain.size()); }

  bool Load(std::uint64_t chunk) {
    if (cached_chunk == chunk)
      return true;
    cached_chunk = std::numeric_limits<std::uint64_t>::max();
    const auto size = static_cast<std::size_t>(
        std::min<std::uint64_t>(kChunkSize, entry.size - chunk * kChunkSize));
    if (!source->seek(source, entry.offset + chunk * (kChunkSize + kTagSize)) ||
        !ReadExact(source, plain.data(), size))
      return false;
    std::uint8_t tag[kTagSize], nonce[kNonceSize], ad[16];
    if (!ReadExact(source, tag, sizeof(tag)))
      return false;
    ChunkParameters(nonce, ad, entry.size, chunk);
    if (crypto_aead_unlock(plain.data(), tag, key.bytes.data(), nonce, ad,
                           sizeof(ad), plain.data(), size)) {
      crypto_wipe(plain.data(), plain.size());
      return Fail();
    }
    cached_chunk = chunk;
    return true;
  }
};

PHYSFS_sint64 PHYSFS_CALL Read(PHYSFS_Io* io,
                               void* buffer,
                               PHYSFS_uint64 size) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  size = std::min<std::uint64_t>(
      {size, stream.entry.size - stream.position,
       static_cast<std::uint64_t>(std::numeric_limits<PHYSFS_sint64>::max()),
       static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())});
  const auto start = stream.position;
  auto* out = static_cast<std::uint8_t*>(buffer);
  while (size) {
    if (!stream.Load(stream.position / kChunkSize)) {
      // A caller can consume the authenticated prefix; retrying at the failed
      // chunk will report its error. Never expose unauthenticated plaintext.
      return stream.position == start ? -1 : stream.position - start;
    }
    const auto in_chunk =
        static_cast<std::size_t>(stream.position % kChunkSize);
    const auto count = static_cast<std::size_t>(
        std::min<std::uint64_t>(size, kChunkSize - in_chunk));
    std::memcpy(out, stream.plain.data() + in_chunk, count);
    stream.position += count;
    out += count;
    size -= count;
  }
  return stream.position - start;
}
PHYSFS_sint64 PHYSFS_CALL Write(PHYSFS_Io*, const void*, PHYSFS_uint64) {
  Fail(PHYSFS_ERR_READ_ONLY);
  return -1;
}
int PHYSFS_CALL Seek(PHYSFS_Io* io, PHYSFS_uint64 position) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  if (position > stream.entry.size)
    return Fail(PHYSFS_ERR_PAST_EOF);
  stream.position = position;
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
                      const Entry& entry,
                      const Key& key,
                      std::uint64_t position = 0);
PHYSFS_Io* PHYSFS_CALL Duplicate(PHYSFS_Io* io) {
  auto& stream = *static_cast<Stream*>(io->opaque);
  auto* source = stream.source->duplicate(stream.source);
  return source ? MakeStream(source, stream.entry, stream.key, stream.position)
                : nullptr;
}
PHYSFS_Io* MakeStream(PHYSFS_Io* source,
                      const Entry& entry,
                      const Key& key,
                      std::uint64_t position) {
  auto* stream = new (std::nothrow) Stream;
  if (!stream) {
    source->destroy(source);
    Fail(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }
  stream->source = source;
  stream->entry = entry;
  stream->key.bytes = key.bytes;
  stream->position = position;
  stream->io = {0,    stream, Read,      Write, Seek,
                Tell, Length, Duplicate, Flush, Destroy};
  return &stream->io;
}

void* PHYSFS_CALL OpenArchive(PHYSFS_Io* io,
                              const char*,
                              int writing,
                              int* claimed) {
  const auto length = io->length(io);
  if (length < 0 || !io->seek(io, 0))
    return nullptr;
  std::uint8_t header[kHeaderSize]{};
  const auto initial = std::min<std::uint64_t>(length, sizeof(kMagic));
  if (!ReadExact(io, header, initial))
    return nullptr;
  if (initial != sizeof(kMagic) ||
      std::memcmp(header, kMagic, sizeof(kMagic))) {
    Fail(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }
  *claimed = 1;
  if (writing) {
    Fail(PHYSFS_ERR_READ_ONLY);
    return nullptr;
  }
  if (length < kHeaderSize + 4 + kTagSize ||
      !ReadExact(io, header + sizeof(kMagic), kHeaderSize - sizeof(kMagic))) {
    Fail();
    return nullptr;
  }
  if (Get(header + 8, 4) != kVersion || Get(header + 12, 4) != kChunkSize) {
    Fail(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }
  const auto index_size = Get(header + 56, 8);
  if (index_size < 4 || index_size > kMaxIndexSize ||
      index_size >
          static_cast<std::uint64_t>(length) - kHeaderSize - kTagSize) {
    Fail();
    return nullptr;
  }
  auto archive = std::unique_ptr<Archive>(new (std::nothrow) Archive);
  if (!archive) {
    Fail(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }
  {
    std::lock_guard lock(g_password.mutex);
    if (g_password.text.empty()) {
      Fail(PHYSFS_ERR_BAD_PASSWORD);
      return nullptr;
    }
    if (!DeriveKey(archive->key, g_password.text, header + 16)) {
      Fail(PHYSFS_ERR_OUT_OF_MEMORY);
      return nullptr;
    }
  }
  std::vector<std::uint8_t> index(static_cast<std::size_t>(index_size));
  std::uint8_t tag[kTagSize];
  if (!ReadExact(io, index.data(), index.size()) ||
      !ReadExact(io, tag, sizeof(tag)))
    return nullptr;
  if (crypto_aead_unlock(index.data(), tag, archive->key.bytes.data(),
                         header + 32, header, sizeof(header), index.data(),
                         index.size())) {
    Fail(PHYSFS_ERR_BAD_PASSWORD);
    return nullptr;
  }
  const bool parsed =
      archive->Parse(index, kHeaderSize + index_size + kTagSize, length);
  crypto_wipe(index.data(), index.size());
  if (!parsed)
    return nullptr;
  archive->source = io;
  return archive.release();
}

PHYSFS_EnumerateCallbackResult PHYSFS_CALL
Enumerate(void* opaque,
          const char* dir,
          PHYSFS_EnumerateCallback callback,
          const char* original,
          void* data) {
  const auto& nodes = static_cast<Archive*>(opaque)->nodes;
  const auto found = nodes.find(FoldPath(dir));
  if (found == nodes.end() || !found->second.directory) {
    Fail(found == nodes.end() ? PHYSFS_ERR_NOT_FOUND : PHYSFS_ERR_NOT_A_FILE);
    return PHYSFS_ENUM_ERROR;
  }
  for (const auto& child : found->second.children) {
    const auto result = callback(data, original, child.c_str());
    if (result != PHYSFS_ENUM_OK) {
      if (result == PHYSFS_ENUM_ERROR)
        Fail(PHYSFS_ERR_APP_CALLBACK);
      return result;
    }
  }
  return PHYSFS_ENUM_OK;
}
PHYSFS_Io* PHYSFS_CALL OpenRead(void* opaque, const char* path) {
  auto& archive = *static_cast<Archive*>(opaque);
  const auto found = archive.nodes.find(FoldPath(path));
  if (found == archive.nodes.end() || found->second.directory) {
    Fail(found == archive.nodes.end() ? PHYSFS_ERR_NOT_FOUND
                                      : PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }
  Key key;
  FileKey(key, archive.key, found->second.entry.nonce.data());
  auto* source = archive.source->duplicate(archive.source);
  return source ? MakeStream(source, found->second.entry, key) : nullptr;
}
PHYSFS_Io* PHYSFS_CALL OpenWrite(void*, const char*) {
  Fail(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}
int PHYSFS_CALL ChangeDirectory(void*, const char*) {
  return Fail(PHYSFS_ERR_READ_ONLY);
}
int PHYSFS_CALL Stat(void* opaque, const char* path, PHYSFS_Stat* stat) {
  const auto& nodes = static_cast<Archive*>(opaque)->nodes;
  const auto found = nodes.find(FoldPath(path));
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

const PHYSFS_Archiver kArchiver = {
    0,
    {"EDB", "URGE uncompressed password-protected bundle (v1)", "URGE", "", 0},
    OpenArchive,
    Enumerate,
    OpenRead,
    OpenWrite,
    OpenWrite,
    ChangeDirectory,
    ChangeDirectory,
    Stat,
    CloseArchive};
}  // namespace

bool Register() {
  return PHYSFS_registerArchiver(&kArchiver) != 0;
}

bool SetPassword(const std::string& password) {
  if (password.size() > kMaxName)
    return Fail(PHYSFS_ERR_INVALID_ARGUMENT);
  std::lock_guard lock(g_password.mutex);
  crypto_wipe(g_password.text.data(), g_password.text.size());
  g_password.text = password;
  return true;
}
}  // namespace urge::urge_bundle
