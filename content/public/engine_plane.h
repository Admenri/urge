// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_PLANE_H_
#define CONTENT_PUBLIC_ENGINE_PLANE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:Plane)--*/
class URGE_RUNTIME_API Plane : public base::RefCounted<Plane> {
 public:
  virtual ~Plane() = default;

  /*--urge(name:initialize,optional:viewport=nullptr)--*/
  static scoped_refptr<Plane> New(ExecutionContext* execution_context,
                                  scoped_refptr<Viewport> viewport,
                                  ExceptionState& exception_state);

  /*--urge(name:set_label)--*/
  virtual void SetLabel(const base::String& label,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:bitmap)--*/
  URGE_EXPORT_ATTRIBUTE(Bitmap, scoped_refptr<Bitmap>);

  /*--urge(name:src_rect)--*/
  URGE_EXPORT_ATTRIBUTE(SrcRect, scoped_refptr<Rect>);

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge(name:ox)--*/
  URGE_EXPORT_ATTRIBUTE(Ox, int32_t);

  /*--urge(name:oy)--*/
  URGE_EXPORT_ATTRIBUTE(Oy, int32_t);

  /*--urge(name:zoom_x)--*/
  URGE_EXPORT_ATTRIBUTE(ZoomX, float);

  /*--urge(name:zoom_y)--*/
  URGE_EXPORT_ATTRIBUTE(ZoomY, float);

  /*--urge(name:opacity)--*/
  URGE_EXPORT_ATTRIBUTE(Opacity, int32_t);

  /*--urge(name:blend_type)--*/
  URGE_EXPORT_ATTRIBUTE(BlendType, int32_t);

  /*--urge(name:color)--*/
  URGE_EXPORT_ATTRIBUTE(Color, scoped_refptr<Color>);

  /*--urge(name:tone)--*/
  URGE_EXPORT_ATTRIBUTE(Tone, scoped_refptr<Tone>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_PLANE_H_
