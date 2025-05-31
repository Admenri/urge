// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/table_impl.h"

namespace content {

scoped_refptr<Table> Table::New(ExecutionContext* execution_context,
                                uint32_t xsize,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<TableImpl>(xsize, 1, 1);
}

scoped_refptr<Table> Table::New(ExecutionContext* execution_context,
                                uint32_t xsize,
                                uint32_t ysize,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<TableImpl>(xsize, ysize, 1);
}

scoped_refptr<Table> Table::New(ExecutionContext* execution_context,
                                uint32_t xsize,
                                uint32_t ysize,
                                uint32_t zsize,
                                ExceptionState& exception_state) {
  return base::MakeRefCounted<TableImpl>(xsize, ysize, zsize);
}

scoped_refptr<Table> Table::Copy(ExecutionContext* execution_context,
                                 scoped_refptr<Table> other,
                                 ExceptionState& exception_state) {
  return base::MakeRefCounted<TableImpl>(*static_cast<TableImpl*>(other.get()));
}

scoped_refptr<Table> Table::Deserialize(ExecutionContext* execution_context,
                                        const base::String& data,
                                        ExceptionState& exception_state) {
  const uint32_t* raw_ptr = reinterpret_cast<const uint32_t*>(data.data());

  uint32_t xsize = *(raw_ptr + 1);
  uint32_t ysize = *(raw_ptr + 2);
  uint32_t zsize = *(raw_ptr + 3);
  uint32_t data_size = *(raw_ptr + 4);

  scoped_refptr<TableImpl> impl =
      base::MakeRefCounted<TableImpl>(xsize, ysize, zsize);
  if (data_size != impl->x_size_ * impl->y_size_ * impl->z_size_) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid table serialize data.");
    return nullptr;
  }

  if (data_size)
    std::memcpy(impl->data_.data(), raw_ptr + 5, data_size * sizeof(int16_t));

  return impl;
}

base::String Table::Serialize(ExecutionContext* execution_context,
                             scoped_refptr<Table> value,
                             ExceptionState& exception_state) {
  scoped_refptr<TableImpl> impl = static_cast<TableImpl*>(value.get());

  uint32_t dim = 0;
  if (impl->x_size_ >= 1)
    dim++;
  if (impl->y_size_ > 1)
    dim++;
  if (impl->z_size_ > 1)
    dim++;

  uint32_t data_size = impl->x_size_ * impl->y_size_ * impl->z_size_;
  base::String serial_data(sizeof(int32_t) * 5 + data_size * sizeof(int16_t), 0);

  uint32_t* ptr = reinterpret_cast<uint32_t*>(serial_data.data());
  *(ptr + 0) = dim;
  *(ptr + 1) = impl->x_size_;
  *(ptr + 2) = impl->y_size_;
  *(ptr + 3) = impl->z_size_;
  *(ptr + 4) = data_size;
  std::memcpy(ptr + 5, impl->data_.data(), data_size * sizeof(int16_t));

  return serial_data;
}

TableImpl::TableImpl(uint32_t xsize, uint32_t ysize, uint32_t zsize)
    : x_size_(xsize),
      y_size_(ysize),
      z_size_(zsize),
      data_(xsize * ysize * zsize) {}

TableImpl::TableImpl(const TableImpl& other)
    : x_size_(other.x_size_),
      y_size_(other.y_size_),
      z_size_(other.z_size_),
      data_(other.data_) {}

scoped_refptr<TableImpl> TableImpl::From(scoped_refptr<Table> host) {
  return static_cast<TableImpl*>(host.get());
}

void TableImpl::Resize(uint32_t xsize, ExceptionState& exception_state) {
  Resize(xsize, y_size_, z_size_, exception_state);
}

void TableImpl::Resize(uint32_t xsize,
                       uint32_t ysize,
                       ExceptionState& exception_state) {
  Resize(xsize, ysize, z_size_, exception_state);
}

void TableImpl::Resize(uint32_t xsize,
                       uint32_t ysize,
                       uint32_t zsize,
                       ExceptionState& exception_state) {
  // Allocate a new table container
  base::Vector<int16_t> new_data(xsize * ysize * zsize);

  // Migrate old data to new container. (Shrink)
  for (uint32_t k = 0; k < std::min(zsize, z_size_); ++k)
    for (uint32_t j = 0; j < std::min(ysize, y_size_); ++j)
      for (uint32_t i = 0; i < std::min(xsize, x_size_); ++i)
        new_data[i + j * x_size_ + k * x_size_ * y_size_] =
            data_.at(i + x_size_ * j + x_size_ * y_size_ * k);

  // Swap data and set new size
  data_.swap(new_data);
  x_size_ = xsize;
  y_size_ = ysize;
  z_size_ = zsize;

  // Notify data modify
  ValueNotification::NotifyObservers();
}

uint32_t TableImpl::Xsize(ExceptionState& exception_state) {
  return x_size_;
}

uint32_t TableImpl::Ysize(ExceptionState& exception_state) {
  return y_size_;
}

uint32_t TableImpl::Zsize(ExceptionState& exception_state) {
  return z_size_;
}

int16_t TableImpl::Get(uint32_t x, ExceptionState& exception_state) {
  return Get(x, 0, 0, exception_state);
}

int16_t TableImpl::Get(uint32_t x,
                       uint32_t y,
                       ExceptionState& exception_state) {
  return Get(x, y, 0, exception_state);
}

int16_t TableImpl::Get(uint32_t x,
                       uint32_t y,
                       uint32_t z,
                       ExceptionState& exception_state) {
  if (x < 0 || x >= x_size_ || y < 0 || y >= y_size_ || z < 0 || z >= z_size_)
    return 0;
  return data_.at(x + x_size_ * (y + y_size_ * z));
}

void TableImpl::Put(uint32_t x,
                    int16_t value,
                    ExceptionState& exception_state) {
  Put(x, 0, 0, value, exception_state);
}

void TableImpl::Put(uint32_t x,
                    uint32_t y,
                    int16_t value,
                    ExceptionState& exception_state) {
  Put(x, y, 0, value, exception_state);
}

void TableImpl::Put(uint32_t x,
                    uint32_t y,
                    uint32_t z,
                    int16_t value,
                    ExceptionState& exception_state) {
  if (x < 0 || x >= x_size_ || y < 0 || y >= y_size_ || z < 0 || z >= z_size_)
    return;

  data_[x + x_size_ * (y + y_size_ * z)] = value;

  // Notify data modify
  ValueNotification::NotifyObservers();
}

uint32_t TableImpl::x_size() {
  return x_size_;
}

uint32_t TableImpl::y_size() {
  return y_size_;
}

uint32_t TableImpl::z_size() {
  return z_size_;
}

int16_t TableImpl::value(uint32_t x, uint32_t y, uint32_t z) {
  return data_.at(x + x_size_ * (y + y_size_ * z));
}

}  // namespace content
