// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_CUBISMSPRITE_H_
#define CONTENT_PUBLIC_ENGINE_CUBISMSPRITE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:CubismSprite)--*/
class URGE_RUNTIME_API CubismSprite : public base::RefCounted<CubismSprite> {
 public:
  virtual ~CubismSprite() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<CubismSprite> New(ExecutionContext* execution_context,
                                         const base::String& model_dir,
                                         const base::String& model_json,
                                         ExceptionState& exception_state);

  /*--urge(name:set_label)--*/
  virtual void SetLabel(const base::String& label,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:set_dragging)--*/
  virtual void SetDragging(float x,
                           float y,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:set_expression)--*/
  virtual void SetExpression(const base::String& expression_name,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:set_random_expression)--*/
  virtual void SetRandomExpression(ExceptionState& exception_state) = 0;

  /*--urge(name:start_motion)--*/
  virtual void StartMotion(const base::String& group,
                           int32_t no,
                           int32_t priority,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:start_random_motion)--*/
  virtual void StartRandomMotion(const base::String& group,
                                 int32_t priority,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:hit_test)--*/
  virtual bool HitTest(const base::String& area_name,
                       float x,
                       float y,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:get_parameter)--*/
  virtual float GetParameter(const base::String& name,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:set_parameter)--*/
  virtual void SetParameter(const base::String& name,
                            float value,
                            float weight,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:add_parameter)--*/
  virtual void AddParameter(const base::String& name,
                            float value,
                            float weight,
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

  /*--urge(name:opacity)--*/
  URGE_EXPORT_ATTRIBUTE(Opacity, int32_t);

  /*--urge(name:blend_type)--*/
  URGE_EXPORT_ATTRIBUTE(BlendType, int32_t);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_CUBISMSPRITE_H_
