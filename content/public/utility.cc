// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/public/utility.h"

namespace content {

std::string Rect::Serialize() {
  std::string data;

  data.resize(4 * sizeof(int32_t));
  Serializable::WriteInt32(data.data(), sizeof(int32_t) * 0, data_.x);
  Serializable::WriteInt32(data.data(), sizeof(int32_t) * 1, data_.y);
  Serializable::WriteInt32(data.data(), sizeof(int32_t) * 2, data_.width);
  Serializable::WriteInt32(data.data(), sizeof(int32_t) * 3, data_.height);

  return data;
}

scoped_refptr<Rect> Rect::Deserialize(const std::string& data) {
  int x, y, w, h;
  x = Serializable::ReadInt32(data.data(), sizeof(int32_t) * 0);
  y = Serializable::ReadInt32(data.data(), sizeof(int32_t) * 1);
  w = Serializable::ReadInt32(data.data(), sizeof(int32_t) * 2);
  h = Serializable::ReadInt32(data.data(), sizeof(int32_t) * 3);

  return new Rect(base::Rect(x, y, w, h));
}

std::string Tone::Serialize() {
  std::string data;

  data.resize(4 * sizeof(double));
  Serializable::WriteDouble(data.data(), sizeof(double) * 0, red_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 1, green_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 2, blue_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 3, gray_);

  return data;
}

scoped_refptr<Tone> Tone::Deserialize(const std::string& data) {
  double x, y, z, w;
  x = Serializable::ReadDouble(data.data(), sizeof(double) * 0);
  y = Serializable::ReadDouble(data.data(), sizeof(double) * 1);
  z = Serializable::ReadDouble(data.data(), sizeof(double) * 2);
  w = Serializable::ReadDouble(data.data(), sizeof(double) * 3);

  return new Tone(x, y, z, w);
}

std::string Color::Serialize() {
  std::string data;

  data.resize(4 * sizeof(double));
  Serializable::WriteDouble(data.data(), sizeof(double) * 0, red_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 1, green_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 2, blue_);
  Serializable::WriteDouble(data.data(), sizeof(double) * 3, alpha_);

  return data;
}

scoped_refptr<Color> Color::Deserialize(const std::string& data) {
  double x, y, z, w;
  x = Serializable::ReadDouble(data.data(), sizeof(double) * 0);
  y = Serializable::ReadDouble(data.data(), sizeof(double) * 1);
  z = Serializable::ReadDouble(data.data(), sizeof(double) * 2);
  w = Serializable::ReadDouble(data.data(), sizeof(double) * 3);

  return new Color(x, y, z, w);
}

}  // namespace content
