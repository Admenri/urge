// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_WINDOW2_H_
#define CONTENT_PUBLIC_ENGINE_WINDOW2_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:Window2)--*/
class URGE_OBJECT(Window2) {
 public:
  virtual ~Window2() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Window2> New(ExecutionContext* execution_context,
                                    ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Window2> New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Window2> New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    int32_t scale,
                                    ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Window2> New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Window2> New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t scale,
                                    ExceptionState& exception_state);

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:move)--*/
  virtual void Move(int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:open?)--*/
  virtual bool IsOpened(ExceptionState& exception_state) = 0;

  /*--urge(name:close?)--*/
  virtual bool IsClosed(ExceptionState& exception_state) = 0;

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:windowskin)--*/
  URGE_EXPORT_ATTRIBUTE(Windowskin, scoped_refptr<Bitmap>);

  /*--urge(name:contents)--*/
  URGE_EXPORT_ATTRIBUTE(Contents, scoped_refptr<Bitmap>);

  /*--urge(name:cursor_rect)--*/
  URGE_EXPORT_ATTRIBUTE(CursorRect, scoped_refptr<Rect>);

  /*--urge(name:active)--*/
  URGE_EXPORT_ATTRIBUTE(Active, bool);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:arrows_visible)--*/
  URGE_EXPORT_ATTRIBUTE(ArrowsVisible, bool);

  /*--urge(name:pause)--*/
  URGE_EXPORT_ATTRIBUTE(Pause, bool);

  /*--urge(name:x)--*/
  URGE_EXPORT_ATTRIBUTE(X, int32_t);

  /*--urge(name:y)--*/
  URGE_EXPORT_ATTRIBUTE(Y, int32_t);

  /*--urge(name:width)--*/
  URGE_EXPORT_ATTRIBUTE(Width, int32_t);

  /*--urge(name:height)--*/
  URGE_EXPORT_ATTRIBUTE(Height, int32_t);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge(name:ox)--*/
  URGE_EXPORT_ATTRIBUTE(Ox, int32_t);

  /*--urge(name:oy)--*/
  URGE_EXPORT_ATTRIBUTE(Oy, int32_t);

  /*--urge(name:padding)--*/
  URGE_EXPORT_ATTRIBUTE(Padding, int32_t);

  /*--urge(name:padding_bottom)--*/
  URGE_EXPORT_ATTRIBUTE(PaddingBottom, int32_t);

  /*--urge(name:opacity)--*/
  URGE_EXPORT_ATTRIBUTE(Opacity, int32_t);

  /*--urge(name:back_opacity)--*/
  URGE_EXPORT_ATTRIBUTE(BackOpacity, int32_t);

  /*--urge(name:contents_opacity)--*/
  URGE_EXPORT_ATTRIBUTE(ContentsOpacity, int32_t);

  /*--urge(name:openness)--*/
  URGE_EXPORT_ATTRIBUTE(Openness, int32_t);

  /*--urge(name:tone)--*/
  URGE_EXPORT_ATTRIBUTE(Tone, scoped_refptr<Tone>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_WINDOW2_H_
