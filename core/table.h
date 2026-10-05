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

#pragma once

#include <vector>

#include "core/definition.h"
#include "core/object.h"

namespace urge {

URGE_BINDING()
class Table : public Object {
 public:
  URGE_BINDING()
  Table(int32_t xsize, int32_t ysize = 1, int32_t zsize = 1);
  URGE_BINDING()
  Table(RefPtr<Table> other);

  URGE_BINDING()
  MARSHAL_DUMP(Table);
  URGE_BINDING()
  MARSHAL_LOAD(Table);

  URGE_BINDING()
  void Resize(int32_t xsize, int32_t ysize = 1, int32_t zsize = 1);
  URGE_BINDING()
  int32_t Xsize();
  URGE_BINDING()
  int32_t Ysize();
  URGE_BINDING()
  int32_t Zsize();

  URGE_BINDING(Name : "[]")
  int16_t Get(int32_t x, int32_t y = 0, int32_t z = 0);
  URGE_BINDING(Name : "[]=")
  void Set(int16_t value, int32_t x, int32_t y = 0, int32_t z = 0);

 private:
  int32_t xsize_ = 0, ysize_ = 0, zsize_ = 0;
  std::vector<int16_t> data_;
};

}  // namespace urge
