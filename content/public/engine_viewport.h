// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_VIEWPORT_H_
#define CONTENT_PUBLIC_ENGINE_VIEWPORT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_color.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuresourcebinding.h"
#include "content/public/engine_rect.h"
#include "content/public/engine_tone.h"

namespace content {

/*--urge(name:Viewport)--*/
class URGE_OBJECT(Viewport) {
 public:
  virtual ~Viewport() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Viewport> New(ExecutionContext* execution_context,
                                     ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Viewport> New(ExecutionContext* execution_context,
                                     scoped_refptr<Rect> rect,
                                     ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Viewport> New(ExecutionContext* execution_context,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height,
                                     ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Viewport> New(ExecutionContext* execution_context,
                                     scoped_refptr<Viewport> parent,
                                     scoped_refptr<Rect> rect,
                                     ExceptionState& exception_state);

  /*--urge(name:set_label)--*/
  virtual void SetLabel(const base::String& label,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:flash)--*/
  virtual void Flash(scoped_refptr<Color> color,
                     uint32_t duration,
                     ExceptionState& exception_state) = 0;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:render)--*/
  virtual void Render(scoped_refptr<Bitmap> target,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:rect)--*/
  URGE_EXPORT_ATTRIBUTE(Rect, scoped_refptr<Rect>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge(name:ox)--*/
  URGE_EXPORT_ATTRIBUTE(Ox, int32_t);

  /*--urge(name:oy)--*/
  URGE_EXPORT_ATTRIBUTE(Oy, int32_t);

  /*--urge(name:color)--*/
  URGE_EXPORT_ATTRIBUTE(Color, scoped_refptr<Color>);

  /*--urge(name:tone)--*/
  URGE_EXPORT_ATTRIBUTE(Tone, scoped_refptr<Tone>);

  /*--urge(name:pipeline_state)--*/
  URGE_EXPORT_ATTRIBUTE(PipelineState, scoped_refptr<GPUPipelineState>);

  /*--urge(name:resource_binding)--*/
  URGE_EXPORT_ATTRIBUTE(ResourceBinding, scoped_refptr<GPUResourceBinding>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_VIEWPORT_H_
