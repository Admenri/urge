// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_SPINESPRITE_H_
#define CONTENT_PUBLIC_ENGINE_SPINESPRITE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_spineevent.h"
#include "content/public/engine_viewport.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:SpineSprite)--*/
class URGE_RUNTIME_API SpineSprite : public base::RefCounted<SpineSprite> {
 public:
  virtual ~SpineSprite() = default;

  /*--urge(name:initialize,optional:default_mix=0.0f)--*/
  static scoped_refptr<SpineSprite> New(ExecutionContext* execution_context,
                                        const base::String& atlas_filename,
                                        const base::String& skeleton_filename,
                                        float default_mix,
                                        ExceptionState& exception_state);

  /*--urge(name:set_label)--*/
  virtual void SetLabel(const base::String& label,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:update)--*/
  virtual base::Vector<scoped_refptr<SpineEvent>> Update(
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_animation,optional:loop=true)--*/
  virtual void SetAnimation(int32_t track_index,
                            const base::String& name,
                            bool loop,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:set_animation_alpha)--*/
  virtual void SetAnimationAlpha(int32_t track_index,
                                 float alpha,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:clear_animation)--*/
  virtual void ClearAnimation(int32_t track_index,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_skin)--*/
  virtual void SetSkin(const base::Vector<base::String>& skin_array,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:set_bone_position)--*/
  virtual void SetBonePosition(const base::String& bone_name,
                               float x,
                               float y,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:x)--*/
  URGE_EXPORT_ATTRIBUTE(X, float);

  /*--urge(name:y)--*/
  URGE_EXPORT_ATTRIBUTE(Y, float);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);

  /*--urge(name:zoom_x)--*/
  URGE_EXPORT_ATTRIBUTE(ZoomX, float);

  /*--urge(name:zoom_y)--*/
  URGE_EXPORT_ATTRIBUTE(ZoomY, float);

  /*--urge(name:premultiplied_alpha)--*/
  URGE_EXPORT_ATTRIBUTE(PremultipliedAlpha, bool);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_SPINESPRITE_H_
