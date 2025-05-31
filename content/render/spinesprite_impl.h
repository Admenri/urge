// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_SPINESPRITE_IMPL_H_
#define CONTENT_RENDER_SPINESPRITE_IMPL_H_

#include "base/memory/allocator.h"
#include "components/spine2d/skeleton_renderer.h"
#include "content/context/disposable.h"
#include "content/public/engine_spinesprite.h"
#include "content/screen/renderscreen_impl.h"
#include "content/screen/viewport_impl.h"

namespace content {

class SpineEventImpl : public SpineEvent {
 public:
  SpineEventImpl(spine::EventType type,
                 spine::Event* event,
                 spine::TrackEntry* track_entry);
  ~SpineEventImpl() override;

  SpineEventImpl(const SpineEventImpl&) = delete;
  SpineEventImpl& operator=(const SpineEventImpl&) = delete;

 public:
  Type GetType(ExceptionState& exception_state) override;
  base::String GetName(ExceptionState& exception_state) override;
  int32_t GetTrackIndex(ExceptionState& exception_state) override;
  float GetTime(ExceptionState& exception_state) override;
  int32_t GetIntValue(ExceptionState& exception_state) override;
  float GetFloatValue(ExceptionState& exception_state) override;
  base::String GetStringValue(ExceptionState& exception_state) override;
  float GetVolume(ExceptionState& exception_state) override;
  float GetBalance(ExceptionState& exception_state) override;

 private:
  spine::EventType type_;
  base::String name_;
  int32_t track_index_;
  float time_;
  int32_t int_value_;
  float float_value_;
  base::String string_value_;
  float volume_;
  float balance_;
};

class SpineSpriteImpl : public SpineSprite,
                        public GraphicsChild,
                        public Disposable,
                        public spine::AnimationStateListenerObject {
 public:
  SpineSpriteImpl(
      RenderScreenImpl* screen,
      base::OwnedPtr<spine::Atlas> atlas,
      base::OwnedPtr<spine::DiligentTextureLoader> texture_loader,
      base::OwnedPtr<spine::SkeletonData> skeleton_data,
      base::OwnedPtr<spine::AnimationStateData> animation_state_data);
  ~SpineSpriteImpl() override;

  SpineSpriteImpl(const SpineSpriteImpl&) = delete;
  SpineSpriteImpl& operator=(const SpineSpriteImpl&) = delete;

 public:
  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  base::Vector<scoped_refptr<SpineEvent>> Update(
      ExceptionState& exception_state) override;
  void SetAnimation(int32_t track_index,
                    const base::String& name,
                    bool loop,
                    ExceptionState& exception_state) override;
  void SetAnimationAlpha(int32_t track_index,
                         float alpha,
                         ExceptionState& exception_state) override;
  void ClearAnimation(int32_t track_index,
                      ExceptionState& exception_state) override;
  void SetSkin(const base::Vector<base::String>& skin_array,
               ExceptionState& exception_state) override;
  void SetBonePosition(const base::String& bone_name,
                       float x,
                       float y,
                       ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(X, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Y, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomX, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomY, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(PremultipliedAlpha, bool);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "SpineSprite"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  void callback(spine::AnimationState* state,
                spine::EventType type,
                spine::TrackEntry* entry,
                spine::Event* event) override;

  DrawableNode node_;

  base::OwnedPtr<spine::Atlas> atlas_;
  base::OwnedPtr<spine::DiligentTextureLoader> texture_loader_;
  base::OwnedPtr<spine::SkeletonData> skeleton_data_;
  base::OwnedPtr<spine::AnimationStateData> animation_state_data_;

  base::OwnedPtr<spine::Skeleton> skeleton_;
  base::OwnedPtr<spine::AnimationState> animation_state_;
  base::OwnedPtr<spine::DiligentRenderer> renderer_;
  base::OwnedPtr<spine::Skin> skin_;

  uint64_t last_ticks_;
  base::Vector<scoped_refptr<SpineEvent>> queued_event_;

  scoped_refptr<ViewportImpl> viewport_;
  bool premultiplied_alpha_;
};

}  // namespace content

#endif  //! CONTENT_RENDER_SPINESPRITE_IMPL_H_
