// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/serializable.h"

namespace content {

int32_t Serializable::ReadInt32(const char* data, int offset) {
  int32_t result;
  std::memcpy(&result, data + offset, sizeof(int32_t));
  return result;
}

double Serializable::ReadDouble(const char* data, int offset) {
  double result;
  std::memcpy(&result, data + offset, sizeof(double));
  return result;
}

void Serializable::WriteInt32(char* data, int offset, int32_t value) {
  std::memcpy(data + offset, &value, sizeof(int32_t));
}

void Serializable::WriteDouble(char* data, int offset, double value) {
  std::memcpy(data + offset, &value, sizeof(double));
}

}  // namespace content
