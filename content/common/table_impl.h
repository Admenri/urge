// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TABLE_IMPL_H_
#define CONTENT_COMMON_TABLE_IMPL_H_

#include <stdint.h>
#include <vector>

#include "content/common/value_observer.h"
#include "content/public/engine_table.h"

namespace content {

class TableImpl : public Table, public ValueNotification {
 public:
  TableImpl(uint32_t xsize, uint32_t ysize = 1, uint32_t zsize = 1);
  TableImpl(const TableImpl& other);
  ~TableImpl() override = default;

  static scoped_refptr<TableImpl> From(scoped_refptr<Table> host);

  void Resize(uint32_t xsize, ExceptionState& exception_state) override;
  void Resize(uint32_t xsize,
              uint32_t ysize,
              ExceptionState& exception_state) override;
  void Resize(uint32_t xsize,
              uint32_t ysize,
              uint32_t zsize,
              ExceptionState& exception_state) override;
  uint32_t Xsize(ExceptionState& exception_state) override;
  uint32_t Ysize(ExceptionState& exception_state) override;
  uint32_t Zsize(ExceptionState& exception_state) override;

  int16_t Get(uint32_t x, ExceptionState& exception_state) override;
  int16_t Get(uint32_t x, uint32_t y, ExceptionState& exception_state) override;
  int16_t Get(uint32_t x,
              uint32_t y,
              uint32_t z,
              ExceptionState& exception_state) override;

  void Put(uint32_t x, int16_t value, ExceptionState& exception_state) override;
  void Put(uint32_t x,
           uint32_t y,
           int16_t value,
           ExceptionState& exception_state) override;
  void Put(uint32_t x,
           uint32_t y,
           uint32_t z,
           int16_t value,
           ExceptionState& exception_state) override;

  uint32_t x_size();
  uint32_t y_size();
  uint32_t z_size();
  int16_t value(uint32_t x, uint32_t y = 0, uint32_t z = 0);

 private:
  friend class Table;

  uint32_t x_size_, y_size_, z_size_;
  std::vector<int16_t> data_;
};

}  // namespace content

#endif  //! CONTENT_COMMON_TABLE_IMPL_H_
