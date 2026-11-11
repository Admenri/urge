// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_widgets.h"

#include "core/ui/ui.h"
#include "core/ui/ui_raycaster.h"
#include "glm/gtc/matrix_transform.hpp"
namespace urge {
UICanvas::UICanvas(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Canvas, this)) {
  SetupTrait(this);
  raycaster_ = MakeRefCounted<UIRaycaster>(this, true);
  UI::Get().RegisterCanvas(this);
}
UICanvas::~UICanvas() { Dispose(); }
glm::mat4 UICanvas::layout_matrix() const {
  return state_->rect_transform_->matrix() *
         glm::scale(glm::mat4(1), glm::vec3(state_->canvas_scale, 1));
}
glm::mat4 UICanvas::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UICanvas::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UICanvas::DoDraw(DrawParam param) { return state_->Draw(param); }
void UICanvas::DisposeObject() {
  if (UI::TryGet()) UI::Get().UnregisterCanvas(this);
  if (raycaster_) raycaster_->Detach();
  state_->Detach();
  Node::DisposeObject();
}
RefPtr<UIRaycaster> UICanvas::Raycaster() const { return raycaster_; }
UIPanel::UIPanel(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Panel, this)) {
  SetupTrait(this);
}
UIPanel::~UIPanel() { Dispose(); }
glm::mat4 UIPanel::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIPanel::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIPanel::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIPanel::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIPanel::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIImage::UIImage(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Image, this)) {
  SetupTrait(this);
}
UIImage::~UIImage() { Dispose(); }
glm::mat4 UIImage::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIImage::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIImage::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIImage::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIImage::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIText::UIText(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Text, this)) {
  SetupTrait(this);
}
UIText::~UIText() { Dispose(); }
glm::mat4 UIText::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIText::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIText::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIText::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIText::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIButton::UIButton(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Button, this)) {
  SetupTrait(this);
}
UIButton::~UIButton() { Dispose(); }
glm::mat4 UIButton::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIButton::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIButton::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIButton::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIButton::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIToggle::UIToggle(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Toggle, this)) {
  SetupTrait(this);
}
UIToggle::~UIToggle() { Dispose(); }
glm::mat4 UIToggle::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIToggle::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIToggle::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIToggle::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIToggle::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UISlider::UISlider(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Slider, this)) {
  SetupTrait(this);
}
UISlider::~UISlider() { Dispose(); }
glm::mat4 UISlider::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UISlider::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UISlider::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UISlider::DoDraw(DrawParam param) { return state_->Draw(param); }
void UISlider::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIProgressBar::UIProgressBar(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::ProgressBar, this)) {
  SetupTrait(this);
}
UIProgressBar::~UIProgressBar() { Dispose(); }
glm::mat4 UIProgressBar::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIProgressBar::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIProgressBar::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIProgressBar::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIProgressBar::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIScrollPanel::UIScrollPanel(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::ScrollPanel, this)) {
  SetupTrait(this);
}
UIScrollPanel::~UIScrollPanel() { Dispose(); }
glm::mat4 UIScrollPanel::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIScrollPanel::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIScrollPanel::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIScrollPanel::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIScrollPanel::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIInputField::UIInputField(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::InputField, this)) {
  SetupTrait(this);
}
UIInputField::~UIInputField() { Dispose(); }
glm::mat4 UIInputField::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIInputField::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIInputField::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIInputField::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIInputField::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UILayoutGroup::UILayoutGroup(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::LayoutGroup, this)) {
  SetupTrait(this);
}
UILayoutGroup::~UILayoutGroup() { Dispose(); }
glm::mat4 UILayoutGroup::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UILayoutGroup::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UILayoutGroup::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UILayoutGroup::DoDraw(DrawParam param) { return state_->Draw(param); }
void UILayoutGroup::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIDropdown::UIDropdown(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Dropdown, this)) {
  SetupTrait(this);
}
UIDropdown::~UIDropdown() { Dispose(); }
glm::mat4 UIDropdown::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIDropdown::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIDropdown::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIDropdown::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIDropdown::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIList::UIList(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::List, this)) {
  SetupTrait(this);
}
UIList::~UIList() { Dispose(); }
glm::mat4 UIList::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIList::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIList::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIList::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIList::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIDialog::UIDialog(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Dialog, this)) {
  SetupTrait(this);
}
UIDialog::~UIDialog() { Dispose(); }
glm::mat4 UIDialog::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIDialog::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIDialog::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIDialog::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIDialog::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UIScrollBar::UIScrollBar(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::ScrollBar, this)) {
  SetupTrait(this);
}
UIScrollBar::~UIScrollBar() { Dispose(); }
glm::mat4 UIScrollBar::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UIScrollBar::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UIScrollBar::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UIScrollBar::DoDraw(DrawParam param) { return state_->Draw(param); }
void UIScrollBar::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
UISeparator::UISeparator(RefPtr<Node> parent)
    : Node(parent, ZValue()),
      state_(MakeRefCounted<UIState>(UIKind::Separator, this)) {
  SetupTrait(this);
}
UISeparator::~UISeparator() { Dispose(); }
glm::mat4 UISeparator::layout_matrix() const {
  return state_->rect_transform_->matrix();
}
glm::mat4 UISeparator::child_matrix() const {
  return state_->kind == UIKind::ScrollPanel
             ? glm::translate(glm::mat4(1),
                              glm::vec3(-state_->scroll_->data, 0))
             : glm::mat4(1);
}
bool UISeparator::Prepare(DrawParam param) { return state_->Prepare(param); }
bool UISeparator::DoDraw(DrawParam param) { return state_->Draw(param); }
void UISeparator::DisposeObject() {
  state_->Detach();
  Node::DisposeObject();
}
}  // namespace urge
