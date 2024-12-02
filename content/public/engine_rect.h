// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_RECT_H_
#define CONTENT_PUBLIC_ENGINE_RECT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
// Interface referrence: RPGVXAce.chm
/*--urge()--*/
class URGE_RUNTIME_API Rect : public virtual base::RefCounted<Rect> {
 public:
  virtual ~Rect() = default;

  /*--urge()--*/
  static scoped_refptr<Rect> New();

  /*--urge()--*/
  static scoped_refptr<Rect> New(int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height);

  /*--urge()--*/
  virtual void Set(int32_t x, int32_t y, int32_t width, int32_t height) = 0;

  /*--urge()--*/
  virtual void Set(scoped_refptr<Rect> other) = 0;

  /*--urge()--*/
  virtual void Empty() = 0;

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(X, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Y, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Width, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Height, int32_t);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_RECT_H_
