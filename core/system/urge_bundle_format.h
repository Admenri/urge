// Licensed under the MIT License. SPDX-License-Identifier: MIT
// Copyright (c) 2026 URGE contributors.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "monocypher.h"

namespace urge::urge_bundle::format {

constexpr char kMagic[8] = {'U', 'R', 'G', 'E', 'E', 'D', 'B', 0};
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kHeaderSize = 64, kKeySize = 32, kSaltSize = 16;
constexpr std::size_t kNonceSize = 24, kTagSize = 16, kChunkSize = 65536;
constexpr std::size_t kMaxName = 4096, kMaxNodes = 1 << 20;
constexpr std::size_t kMaxIndexSize = 128 * 1024 * 1024;
constexpr std::uint32_t kKdfBlocks = 32768, kKdfPasses = 3;

inline std::uint64_t Get(const std::uint8_t* data, unsigned bytes) {
  std::uint64_t value = 0;
  for (unsigned i = 0; i < bytes; ++i)
    value |= std::uint64_t(data[i]) << (i * 8);
  return value;
}
inline void Put(std::uint8_t* data, std::uint64_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i)
    data[i] = static_cast<std::uint8_t>(value >> (i * 8));
}

struct Key {
  std::array<std::uint8_t, kKeySize> bytes{};
  ~Key() { crypto_wipe(bytes.data(), bytes.size()); }
};

inline bool DeriveKey(Key& key,
                      const std::string& password,
                      const std::uint8_t* salt) {
  if (password.empty() || password.size() > kMaxName)
    return false;
  auto work = std::unique_ptr<std::uint8_t[]>(
      new (std::nothrow) std::uint8_t[kKdfBlocks * 1024]);
  if (!work)
    return false;
  crypto_argon2_config config{CRYPTO_ARGON2_ID, kKdfBlocks, kKdfPasses, 1};
  crypto_argon2_inputs inputs{
      reinterpret_cast<const std::uint8_t*>(password.data()), salt,
      static_cast<std::uint32_t>(password.size()), kSaltSize};
  crypto_argon2(key.bytes.data(), kKeySize, work.get(), config, inputs,
                crypto_argon2_no_extras);
  // Monocypher wipes the work area itself.
  return true;
}

inline std::uint64_t ChunkCount(std::uint64_t size) {
  return size / kChunkSize + (size % kChunkSize != 0);
}

// Each resource has an independent subkey; chunk numbers use independent
// nonces. Size and chunk number are authenticated to prevent substitutions.
inline void FileKey(Key& key,
                    const Key& archive_key,
                    const std::uint8_t* nonce) {
  crypto_blake2b_keyed(key.bytes.data(), kKeySize, archive_key.bytes.data(),
                       kKeySize, nonce, kNonceSize);
}
inline void ChunkParameters(std::uint8_t nonce[kNonceSize],
                            std::uint8_t ad[16],
                            std::uint64_t size,
                            std::uint64_t chunk) {
  std::memset(nonce, 0, kNonceSize);
  Put(nonce, chunk, 8);
  Put(ad, size, 8);
  Put(ad + 8, chunk, 8);
}

// Canonical relative UTF-8 paths, with ASCII-insensitive lookup like RGSS.
inline std::string FoldPath(std::string path) {
  for (char& ch : path) {
    if (ch == '\\')
      ch = '/';
    if (ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';
  }
  return path;
}
inline bool ValidName(const std::string& name) {
  if (name.empty() || name.size() > kMaxName || name.front() == '/' ||
      name.back() == '/')
    return false;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= name.size(); ++i) {
    if (i == name.size() || name[i] == '/') {
      const auto length = i - start;
      if (!length || (length == 1 && name[start] == '.') ||
          (length == 2 && name[start] == '.' && name[start + 1] == '.'))
        return false;
      start = i + 1;
    } else if (static_cast<unsigned char>(name[i]) < 32 || name[i] == ':' ||
               name[i] == '\\')
      return false;
  }
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

}  // namespace urge::urge_bundle::format
