// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_DRAWABLE_H_
#define CONTENT_PUBLIC_ENGINE_DRAWABLE_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpurenderdevice.h"
#include "content/public/engine_gputexture.h"
#include "content/public/engine_rect.h"
#include "content/public/engine_viewport.h"

namespace content {

/*--urge(name:RenderScissorStack)--*/
class URGE_OBJECT(RenderScissorStack) {
 public:
  virtual ~RenderScissorStack() = default;

  /*--urge(name:current)--*/
  virtual scoped_refptr<Rect> GetCurrent(ExceptionState& exception_state) = 0;

  /*--urge(name:push)--*/
  virtual void Push(scoped_refptr<Rect> region,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:pop)--*/
  virtual void Pop(ExceptionState& exception_state) = 0;

  /*--urge(name:reset)--*/
  virtual void Reset(ExceptionState& exception_state) = 0;
};

/*--urge(name:DrawableRenderParams)--*/
struct URGE_OBJECT(DrawableRenderParams) {
  scoped_refptr<GPUDeviceContext> context;
  scoped_refptr<GPUTexture> screen_buffer;
  scoped_refptr<GPUTexture> screen_depth_stencil;
  int32_t screen_width = 0;
  int32_t screen_height = 0;
  scoped_refptr<RenderScissorStack> scissors_stack;
  scoped_refptr<GPUBuffer> root_world;
  scoped_refptr<GPUBuffer> world_binding;
};

/*--urge(name:Drawable)--*/
class URGE_OBJECT(Drawable) {
 public:
  virtual ~Drawable() = default;

  /*--urge(name:RenderCallback)--*/
  using RenderCallback =
      base::RepeatingCallback<void(scoped_refptr<DrawableRenderParams> params)>;

  /*--urge(name:Stage)--*/
  enum Stage {
    STAGE_BEFORE_RENDER = 0,
    STAGE_ON_RENDERING,
    STAGE_NOTIFICATION,
    STAGE_NUMS,
  };

  /*--urge(name:initialize,optional:viewport=nullptr)--*/
  static scoped_refptr<Drawable> New(ExecutionContext* execution_context,
                                     scoped_refptr<Viewport> viewport,
                                     ExceptionState& exception_state);

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:parent_rect)--*/
  virtual scoped_refptr<Rect> GetParentRect(
      ExceptionState& exception_state) = 0;

  /*--urge(name:parent_ox)--*/
  virtual int32_t GetParentOX(ExceptionState& exception_state) = 0;

  /*--urge(name:parent_oy)--*/
  virtual int32_t GetParentOY(ExceptionState& exception_state) = 0;

  /*--urge(name:setup_render)--*/
  virtual void SetupRender(Stage stage,
                           RenderCallback callback,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:viewport)--*/
  URGE_EXPORT_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);

  /*--urge(name:visible)--*/
  URGE_EXPORT_ATTRIBUTE(Visible, bool);

  /*--urge(name:z)--*/
  URGE_EXPORT_ATTRIBUTE(Z, int32_t);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_DRAWABLE_H_
