// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_RECT_H_
#define CONTENT_PUBLIC_ENGINE_RECT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
// Interface reference: RGSS Reference
/*--urge(name:Rect)--*/
class URGE_RUNTIME_API Rect : public base::RefCounted<Rect> {
 public:
  virtual ~Rect() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Rect> New(ExecutionContext* execution_context,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Rect> New(ExecutionContext* execution_context,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Rect> Copy(ExecutionContext* execution_context,
                                  scoped_refptr<Rect> other,
                                  ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Rect);

  /*--urge(comparable)--*/
  URGE_EXPORT_COMPARABLE(Rect);

  /*--urge(name:set)--*/
  virtual void Set(int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:set)--*/
  virtual void Set(scoped_refptr<Rect> other,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:empty)--*/
  virtual void Empty(ExceptionState& exception_state) = 0;

  /*--urge(name:x)--*/
  URGE_EXPORT_ATTRIBUTE(X, int32_t);

  /*--urge(name:y)--*/
  URGE_EXPORT_ATTRIBUTE(Y, int32_t);

  /*--urge(name:width)--*/
  URGE_EXPORT_ATTRIBUTE(Width, int32_t);

  /*--urge(name:height)--*/
  URGE_EXPORT_ATTRIBUTE(Height, int32_t);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_RECT_H_
