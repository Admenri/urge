// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_TABLE_H_
#define CONTENT_PUBLIC_ENGINE_TABLE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:Table)--*/
class URGE_OBJECT(Table) {
 public:
  virtual ~Table() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Table> New(ExecutionContext* execution_context,
                                  uint32_t xsize,
                                  ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Table> New(ExecutionContext* execution_context,
                                  uint32_t xsize,
                                  uint32_t ysize,
                                  ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Table> New(ExecutionContext* execution_context,
                                  uint32_t xsize,
                                  uint32_t ysize,
                                  uint32_t zsize,
                                  ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Table> Copy(ExecutionContext* execution_context,
                                   scoped_refptr<Table> other,
                                   ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Table);

  /*--urge(name:resize)--*/
  virtual void Resize(uint32_t xsize, ExceptionState& exception_state) = 0;

  /*--urge(name:resize)--*/
  virtual void Resize(uint32_t xsize,
                      uint32_t ysize,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:resize)--*/
  virtual void Resize(uint32_t xsize,
                      uint32_t ysize,
                      uint32_t zsize,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:xsize)--*/
  virtual uint32_t Xsize(ExceptionState& exception_state) = 0;

  /*--urge(name:ysize)--*/
  virtual uint32_t Ysize(ExceptionState& exception_state) = 0;

  /*--urge(name:zsize)--*/
  virtual uint32_t Zsize(ExceptionState& exception_state) = 0;

  /*--urge(name:[])--*/
  virtual int16_t Get(uint32_t x, ExceptionState& exception_state) = 0;

  /*--urge(name:[])--*/
  virtual int16_t Get(uint32_t x,
                      uint32_t y,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:[])--*/
  virtual int16_t Get(uint32_t x,
                      uint32_t y,
                      uint32_t z,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:[]=)--*/
  virtual void Put(uint32_t x,
                   int16_t value,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:[]=)--*/
  virtual void Put(uint32_t x,
                   uint32_t y,
                   int16_t value,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:[]=)--*/
  virtual void Put(uint32_t x,
                   uint32_t y,
                   uint32_t z,
                   int16_t value,
                   ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_RECT_H_
