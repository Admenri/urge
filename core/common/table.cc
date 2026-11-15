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

#include "core/common/table.h"

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/common/object.h"

namespace urge {

Table::Table(int32_t xsize, int32_t ysize, int32_t zsize) {
  constexpr int64_t kMaxElements = 1 << 26;
  const int64_t elements = static_cast<int64_t>(xsize) * ysize * zsize;
  if (xsize < 0 || ysize < 0 || zsize < 0 || elements > kMaxElements) {
    LOGGER_ERROR("[Table] {}x{}x{} is not a valid size; using 0x0x1", xsize,
                 ysize, zsize);
    xsize = 0;
    ysize = 0;
    zsize = 1;
  }
  xsize_ = xsize;
  ysize_ = ysize;
  zsize_ = zsize;
  data_.assign(static_cast<size_t>(static_cast<int64_t>(xsize) * ysize * zsize),
               0);
}

Table::Table(RefPtr<Table> other)
    : xsize_(other->xsize_),
      ysize_(other->ysize_),
      zsize_(other->zsize_),
      data_(other->data_.begin(), other->data_.end()) {}

MARSHAL_DUMP_DEF(Table) {
  uint32_t dim = 0;
  if (obj->xsize_ >= 1)
    dim++;
  if (obj->ysize_ > 1)
    dim++;
  if (obj->zsize_ > 1)
    dim++;

  uint32_t data_size = obj->xsize_ * obj->ysize_ * obj->zsize_;
  std::string serial_data(sizeof(int32_t) * 5 + data_size * sizeof(int16_t), 0);

  uint32_t* ptr = reinterpret_cast<uint32_t*>(serial_data.data());
  *(ptr + 0) = dim;
  *(ptr + 1) = obj->xsize_;
  *(ptr + 2) = obj->ysize_;
  *(ptr + 3) = obj->zsize_;
  *(ptr + 4) = data_size;
  std::memcpy(ptr + 5, obj->data_.data(), data_size * sizeof(int16_t));

  return serial_data;
}

MARSHAL_LOAD_DEF(Table) {
  const uint32_t* raw_ptr = reinterpret_cast<const uint32_t*>(data.data());

  uint32_t xsize = *(raw_ptr + 1);
  uint32_t ysize = *(raw_ptr + 2);
  uint32_t zsize = *(raw_ptr + 3);
  uint32_t data_size = *(raw_ptr + 4);

  RefPtr<Table> obj = MakeRefCounted<Table>(xsize, ysize, zsize);
  if (data_size != obj->xsize_ * obj->ysize_ * obj->zsize_) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "invalid table serialize data");
    return nullptr;
  }

  if (data_size)
    std::memcpy(obj->data_.data(), raw_ptr + 5, data_size * sizeof(int16_t));

  return obj;
}

void Table::Resize(URGE_EXCEPT, int32_t xsize, int32_t ysize, int32_t zsize) {
  constexpr int64_t kMaxElements = 1 << 26;
  const int64_t elements = static_cast<int64_t>(xsize) * ysize * zsize;
  if (xsize < 0 || ysize < 0 || zsize < 0 || elements > kMaxElements) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "table: {}x{}x{} is not a valid size.", xsize, ysize,
                          zsize);
    return;
  }

  xsize_ = xsize;
  ysize_ = ysize;
  zsize_ = zsize;

  data_.resize(static_cast<size_t>(elements), 0);
  data_.shrink_to_fit();
}

int32_t Table::Xsize() {
  return xsize_;
}

int32_t Table::Ysize() {
  return ysize_;
}

int32_t Table::Zsize() {
  return zsize_;
}

int16_t Table::Get(int32_t x, int32_t y, int32_t z) {
  int32_t index = x + y * xsize_ + z * xsize_ * ysize_;
  return data_.at(index);
}

void Table::Set(int16_t value, int32_t x, int32_t y, int32_t z) {
  int32_t index = x + y * xsize_ + z * xsize_ * ysize_;
  data_.at(index) = value;
}

}
