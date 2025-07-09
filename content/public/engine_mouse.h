// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_MOUSE_H_
#define CONTENT_PUBLIC_ENGINE_MOUSE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"

namespace content {

/*--urge(name:Mouse,is_module)--*/
class URGE_OBJECT(Mouse) {
 public:
  virtual ~Mouse() = default;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:x)--*/
  virtual int32_t GetX(ExceptionState& exception_state) = 0;

  /*--urge(name:y)--*/
  virtual int32_t GetY(ExceptionState& exception_state) = 0;

  /*--urge(name:set_pos)--*/
  virtual void SetPosition(int32_t x,
                           int32_t y,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:down?)--*/
  virtual bool IsDown(int32_t button, ExceptionState& exception_state) = 0;

  /*--urge(name:up?)--*/
  virtual bool IsUp(int32_t button, ExceptionState& exception_state) = 0;

  /*--urge(name:double?)--*/
  virtual bool IsDouble(int32_t button, ExceptionState& exception_state) = 0;

  /*--urge(name:press?)--*/
  virtual bool IsPressed(int32_t button, ExceptionState& exception_state) = 0;

  /*--urge(name:move?)--*/
  virtual bool IsMoved(ExceptionState& exception_state) = 0;

  /*--urge(name:scroll_x)--*/
  virtual int32_t GetScrollX(ExceptionState& exception_state) = 0;

  /*--urge(name:scroll_y)--*/
  virtual int32_t GetScrollY(ExceptionState& exception_state) = 0;

  /*--urge(name:set_cursor,optional:hot_x=0,optional:hot_y=0)--*/
  virtual void SetCursor(scoped_refptr<Bitmap> cursor,
                         int32_t hot_x,
                         int32_t hot_y,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_MOUSE_H_
