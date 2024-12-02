// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_SPRITE_H_
#define CONTENT_PUBLIC_ENGINE_SPRITE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_viewport.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
// Interface referrence: RPGVXAce.chm
/*--urge()--*/
class URGE_RUNTIME_API Sprite : public virtual base::RefCounted<Sprite> {
 public:
  virtual ~Sprite() = default;

  /*--urge()--*/
  static scoped_refptr<Sprite> New(scoped_refptr<Viewport> viewport);

  /*--urge()--*/
  virtual void Dispose() = 0;

  /*--urge()--*/
  virtual bool IsDisposed() = 0;

  /*--urge()--*/
  virtual void Flash(scoped_refptr<Color> color, uint32_t duration) = 0;

  /*--urge()--*/
  virtual void Update() = 0;

  /*--urge()--*/
  virtual uint32_t Width() = 0;

  /*--urge()--*/
  virtual uint32_t Height() = 0;

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Bitmap, scoped_refptr<Bitmap>);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(SrcRect, scoped_refptr<Rect>);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(X, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Y, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Ox, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Oy, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(ZoomX, float);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(ZoomY, float);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Angle, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(WaveAmp, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(WaveLength, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(WaveSpeed, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(WavePhase, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Mirror, bool);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(BushDepth, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(BushOpacity, int32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Opacity, uint32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(BlendType, uint32_t);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Color, scoped_refptr<Color>);

  /*--urge()--*/
  URGE_EXPORT_ATTRIBUTE(Tone, scoped_refptr<Tone>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_SPRITE_H_
