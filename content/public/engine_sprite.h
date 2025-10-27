// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_SPRITE_H_
#define CONTENT_PUBLIC_ENGINE_SPRITE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:Sprite)--*/
class URGE_OBJECT(Sprite) {
 public:
  virtual ~Sprite() = default;

  /*--urge(name:initialize,optional:viewport=nullptr,optional:disable_vertical_sort=false)--*/
  static scoped_refptr<Sprite> New(ExecutionContext* execution_context,
                                   scoped_refptr<Viewport> viewport,
                                   bool disable_vertical_sort,
                                   ExceptionState& exception_state);

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:flash)--*/
  virtual void Flash(scoped_refptr<Color> color,
                     uint32_t duration,
                     ExceptionState& exception_state) = 0;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual uint32_t Width(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual uint32_t Height(ExceptionState& exception_state) = 0;

  /*--urge(name:bitmap)--*/
  URGE_EXPORT_ATTRIBUTE(Bitmap, scoped_refptr<Bitmap>);

  /*--urge(name:src_rect)--*/
  URGE_EXPORT_ATTRIBUTE(SrcRect, scoped_refptr<Rect>);

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:x)--*/
  URGE_EXPORT_ATTRIBUTE(X, int32_t);

  /*--urge(name:y)--*/
  URGE_EXPORT_ATTRIBUTE(Y, int32_t);

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

  /*--urge(name:angle)--*/
  URGE_EXPORT_ATTRIBUTE(Angle, float);

  /*--urge(name:wave_amp)--*/
  URGE_EXPORT_ATTRIBUTE(WaveAmp, int32_t);

  /*--urge(name:wave_length)--*/
  URGE_EXPORT_ATTRIBUTE(WaveLength, int32_t);

  /*--urge(name:wave_speed)--*/
  URGE_EXPORT_ATTRIBUTE(WaveSpeed, int32_t);

  /*--urge(name:wave_phase)--*/
  URGE_EXPORT_ATTRIBUTE(WavePhase, int32_t);

  /*--urge(name:mirror)--*/
  URGE_EXPORT_ATTRIBUTE(Mirror, bool);

  /*--urge(name:bush_depth)--*/
  URGE_EXPORT_ATTRIBUTE(BushDepth, int32_t);

  /*--urge(name:bush_opacity)--*/
  URGE_EXPORT_ATTRIBUTE(BushOpacity, int32_t);

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

#endif  //! CONTENT_PUBLIC_ENGINE_SPRITE_H_
