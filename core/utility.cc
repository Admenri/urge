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

#include "core/utility.h"

namespace urge {

MARSHAL_DUMP_DEF(Rect) {
  std::string serial_data(sizeof(int32_t) * 4, 0);
  std::memcpy(serial_data.data(), &obj->data, sizeof(glm::vec4));
  return serial_data;
}

MARSHAL_LOAD_DEF(Rect) {
  const size_t size = data.size();
  if (size < sizeof(int32_t) * 4)
    throw Exception(Exception::kRGSSError, "invalid data length ({})", size);

  const int32_t* ptr = reinterpret_cast<const int32_t*>(data.data());
  return MakeRefCounted<Rect>(*(ptr + 0), *(ptr + 1), *(ptr + 2), *(ptr + 3));
}

MARSHAL_DUMP_DEF(Color) {
  std::string serial_data(sizeof(double) * 4, 0);

  double* target_ptr = reinterpret_cast<double*>(serial_data.data());
  *(target_ptr + 0) = static_cast<double>(obj->data.r);
  *(target_ptr + 1) = static_cast<double>(obj->data.g);
  *(target_ptr + 2) = static_cast<double>(obj->data.b);
  *(target_ptr + 3) = static_cast<double>(obj->data.a);

  return serial_data;
}

MARSHAL_LOAD_DEF(Color) {
  const size_t size = data.size();
  if (size < sizeof(double) * 4)
    throw Exception(Exception::kRGSSError, "invalid data length ({})", size);

  const double* ptr = reinterpret_cast<const double*>(data.data());
  const float red = static_cast<float>(*(ptr + 0));
  const float green = static_cast<float>(*(ptr + 1));
  const float blue = static_cast<float>(*(ptr + 2));
  const float alpha = static_cast<float>(*(ptr + 3));

  return MakeRefCounted<Color>(red, green, blue, alpha);
}

MARSHAL_DUMP_DEF(Tone) {
  std::string serial_data(sizeof(double) * 4, 0);

  double* target_ptr = reinterpret_cast<double*>(serial_data.data());
  *(target_ptr + 0) = static_cast<double>(obj->data.r);
  *(target_ptr + 1) = static_cast<double>(obj->data.g);
  *(target_ptr + 2) = static_cast<double>(obj->data.b);
  *(target_ptr + 3) = static_cast<double>(obj->data.a);

  return serial_data;
}

MARSHAL_LOAD_DEF(Tone) {
  const size_t size = data.size();
  if (size < sizeof(double) * 4)
    throw Exception(Exception::kRGSSError, "invalid data length ({})", size);

  const double* ptr = reinterpret_cast<const double*>(data.data());
  const float red = static_cast<float>(*(ptr + 0));
  const float green = static_cast<float>(*(ptr + 1));
  const float blue = static_cast<float>(*(ptr + 2));
  const float gray = static_cast<float>(*(ptr + 3));

  return MakeRefCounted<Tone>(red, green, blue, gray);
}

}  // namespace urge
