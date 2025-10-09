// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/drawable_impl.h"

#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/device_context_impl.h"
#include "content/gpu/texture_impl.h"

namespace content {

RenderScissorStackImpl::RenderScissorStackImpl(ScissorStack* stack)
    : stack_(stack) {
  DCHECK(stack_);
}

RenderScissorStackImpl::~RenderScissorStackImpl() = default;

scoped_refptr<Rect> RenderScissorStackImpl::GetCurrent(
    ExceptionState& exception_state) {
  if (!stack_) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid scissor state");
    return nullptr;
  }

  return base::MakeRefCounted<RectImpl>(stack_->Current());
}

void RenderScissorStackImpl::RenderScissorStackImpl::Push(
    scoped_refptr<Rect> region,
    ExceptionState& exception_state) {
  if (!stack_)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid scissor state");

  auto region_obj = RectImpl::From(region);
  if (!region_obj)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid rect object");

  stack_->Push(region_obj->AsBaseRect());
}

void RenderScissorStackImpl::Pop(ExceptionState& exception_state) {
  if (!stack_)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid scissor state");

  stack_->Pop();
}

void RenderScissorStackImpl::Reset(ExceptionState& exception_state) {
  if (!stack_)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid scissor state");

  stack_->Reset();
}

scoped_refptr<Drawable> Drawable::New(ExecutionContext* execution_context,
                                      scoped_refptr<Viewport> viewport,
                                      ExceptionState& exception_state) {
  return base::MakeRefCounted<DrawableImpl>(execution_context,
                                            ViewportImpl::From(viewport));
}

DrawableImpl::DrawableImpl(ExecutionContext* execution_context,
                           scoped_refptr<ViewportImpl> parent)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      node_(parent ? parent->GetDrawableController()
                   : execution_context->screen_drawable_node,
            SortKey()) {
  node_.RegisterEventHandler(base::BindRepeating(
      &DrawableImpl::DrawableNodeHandlerInternal, base::Unretained(this)));
}

DISPOSABLE_DEFINITION(DrawableImpl);

void DrawableImpl::SetLabel(const std::string& label,
                            ExceptionState& exception_state) {
  node_.SetDebugLabel(label);
}

scoped_refptr<Rect> DrawableImpl::GetParentRect(
    ExceptionState& exception_state) {
  auto* parent = node_.GetParentViewport();
  if (!parent)
    return nullptr;
  return base::MakeRefCounted<RectImpl>(parent->bound);
}

int32_t DrawableImpl::GetParentOX(ExceptionState& exception_state) {
  auto* parent = node_.GetParentViewport();
  if (!parent)
    return 0;
  return parent->origin.x;
}

int32_t DrawableImpl::GetParentOY(ExceptionState& exception_state) {
  auto* parent = node_.GetParentViewport();
  if (!parent)
    return 0;
  return parent->origin.y;
}

void DrawableImpl::SetupRender(Stage stage,
                               RenderCallback callback,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (stage >= 0 && stage < STAGE_NUMS)
    callbacks_[stage] = callback;
}

scoped_refptr<Viewport> DrawableImpl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void DrawableImpl::Put_Viewport(const scoped_refptr<Viewport>& value,
                                ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_ ? viewport_->GetDrawableController()
                                   : context()->screen_drawable_node);
}

bool DrawableImpl::Get_Visible(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return node_.GetVisibility();
}

void DrawableImpl::Put_Visible(const bool& value,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeVisibility(value);
}

int32_t DrawableImpl::Get_Z(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return node_.GetSortKeys()->weight[0];
}

void DrawableImpl::Put_Z(const int32_t& value,
                         ExceptionState& exception_state) {
  DISPOSE_CHECK;

  node_.SetNodeSortWeight(value);
}

void DrawableImpl::OnObjectDisposed() {
  node_.DisposeNode();
}

void DrawableImpl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  auto scissor_state =
      base::MakeRefCounted<RenderScissorStackImpl>(params->scissors);

  // Make rendering params
  auto params_obj = base::MakeRefCounted<DrawableRenderParams>();
  params_obj->context =
      base::MakeRefCounted<DeviceContextImpl>(context(), params->context);
  params_obj->screen_buffer =
      base::MakeRefCounted<TextureImpl>(context(), params->screen_buffer);
  params_obj->screen_depth_stencil = base::MakeRefCounted<TextureImpl>(
      context(), params->screen_depth_stencil);
  params_obj->screen_width = params->screen_size.x;
  params_obj->screen_height = params->screen_size.y;
  params_obj->scissors_stack = scissor_state;
  params_obj->root_world =
      base::MakeRefCounted<BufferImpl>(context(), params->root_world);
  params_obj->world_binding =
      base::MakeRefCounted<BufferImpl>(context(), params->world_binding);

  // Call render process
  switch (stage) {
    case DrawableNode::RenderStage::BEFORE_RENDER:
      if (!callbacks_[STAGE_BEFORE_RENDER].is_null())
        callbacks_[STAGE_BEFORE_RENDER].Run(params_obj);
      break;
    case DrawableNode::RenderStage::ON_RENDERING:
      if (!callbacks_[STAGE_ON_RENDERING].is_null())
        callbacks_[STAGE_ON_RENDERING].Run(params_obj);
      break;
    case DrawableNode::RenderStage::NOTIFICATION:
      if (!callbacks_[STAGE_NOTIFICATION].is_null())
        callbacks_[STAGE_NOTIFICATION].Run(params_obj);
      break;
    default:
      break;
  }

  // Reset scissor state after rendering
  scissor_state->ResetState();
}

}  // namespace content
