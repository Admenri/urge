// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_SERIALIZABLE_H_
#define CONTENT_PUBLIC_SERIALIZABLE_H_

#include <stdint.h>
#include <string>

#include "base/memory/ref_counted.h"

namespace content {

class Serializable {
 public:
  Serializable() = default;

  Serializable(const Serializable&) = delete;
  Serializable& operator=(const Serializable&) = delete;

  virtual std::string Serialize() = 0;

  template <class T>
  static scoped_refptr<T> Deserialize(const std::string& data) {
    return T::Deserialize(data);
  }

  static int32_t ReadInt32(const char* data, int offset);
  static double ReadDouble(const char* data, int offset);
  static void WriteInt32(char* data, int offset, int32_t value);
  static void WriteDouble(char* data, int offset, double value);
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_SERIALIZABLE_H_
