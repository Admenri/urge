// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once

#include "core/scene/node.h"
#include "core/ui/ui_state.h"
namespace urge {
class UIRaycaster;
URGE_BINDING()
class UICanvas : public Node {
 public:
  URGE_BINDING()
  UICanvas(MaybePtr<Node> parent = nullptr);
  ~UICanvas() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;
  URGE_BINDING()
  RefPtr<UIRaycaster> Raycaster() const;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
  RefPtr<UIRaycaster> raycaster_;
};
URGE_BINDING()
class UIPanel : public Node {
 public:
  URGE_BINDING()
  UIPanel(MaybePtr<Node> parent = nullptr);
  ~UIPanel() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIImage : public Node {
 public:
  URGE_BINDING()
  UIImage(MaybePtr<Node> parent = nullptr);
  ~UIImage() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIText : public Node {
 public:
  URGE_BINDING()
  UIText(MaybePtr<Node> parent = nullptr);
  ~UIText() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIButton : public Node {
 public:
  URGE_BINDING()
  UIButton(MaybePtr<Node> parent = nullptr);
  ~UIButton() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIToggle : public Node {
 public:
  URGE_BINDING()
  UIToggle(MaybePtr<Node> parent = nullptr);
  ~UIToggle() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UISlider : public Node {
 public:
  URGE_BINDING()
  UISlider(MaybePtr<Node> parent = nullptr);
  ~UISlider() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIProgressBar : public Node {
 public:
  URGE_BINDING()
  UIProgressBar(MaybePtr<Node> parent = nullptr);
  ~UIProgressBar() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIScrollPanel : public Node {
 public:
  URGE_BINDING()
  UIScrollPanel(MaybePtr<Node> parent = nullptr);
  ~UIScrollPanel() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIInputField : public Node {
 public:
  URGE_BINDING()
  UIInputField(MaybePtr<Node> parent = nullptr);
  ~UIInputField() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UILayoutGroup : public Node {
 public:
  URGE_BINDING()
  UILayoutGroup(MaybePtr<Node> parent = nullptr);
  ~UILayoutGroup() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIDropdown : public Node {
 public:
  URGE_BINDING()
  UIDropdown(MaybePtr<Node> parent = nullptr);
  ~UIDropdown() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIList : public Node {
 public:
  URGE_BINDING()
  UIList(MaybePtr<Node> parent = nullptr);
  ~UIList() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIDialog : public Node {
 public:
  URGE_BINDING()
  UIDialog(MaybePtr<Node> parent = nullptr);
  ~UIDialog() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UIScrollBar : public Node {
 public:
  URGE_BINDING()
  UIScrollBar(MaybePtr<Node> parent = nullptr);
  ~UIScrollBar() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
URGE_BINDING()
class UISeparator : public Node {
 public:
  URGE_BINDING()
  UISeparator(MaybePtr<Node> parent = nullptr);
  ~UISeparator() override;
  URGE_BINDING()
  RefPtr<UIState> State() const { return state_; }
  URGE_BINDING()
  RefPtr<UIRectTransform> RectTransform() const { return state_->rect_transform_; }
  UIState* ui_state() const override { return state_.get(); }
  glm::mat4 child_matrix() const override;

 protected:
  glm::mat4 layout_matrix() const override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void DisposeObject() override;

 private:
  RefPtr<UIState> state_;
};
}  // namespace urge
