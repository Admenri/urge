// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/render/primitive.h"
#include "core/scene/drawable.h"
#include "core/scene/node.h"
#include "core/ui/ui_math.h"
#include "core/ui/ui_rect_transform.h"
#include "core/ui/ui_style.h"
namespace urge {
class Node;
class UI;
enum class UIKind {
  Canvas,
  Panel,
  Image,
  Text,
  Button,
  Toggle,
  Slider,
  ProgressBar,
  ScrollPanel,
  InputField,
  LayoutGroup,
  Dropdown,
  List,
  Dialog,
  ScrollBar,
  Separator
};
URGE_BINDING()
class UIState : public Object {
 public:
  URGE_BINDING()
  UIState();
  UIState(UIKind kind, Node* owner);
  ~UIState() override;
  URGE_BINDING()
  ATTR(RefPtr<UIStyle>, Style) {
    if (value.has_value()) style_ = *value;
    return style_;
  }
  URGE_BINDING()
  ATTR(RefPtr<UIRectTransform>, RectTransform) {
    if (value.has_value()) rect_transform_ = *value;
    return rect_transform_;
  }
  URGE_BINDING()
  ATTR(bool, Interactable) {
    if (value.has_value()) interactable_ = *value;
    return interactable_;
  }
  URGE_BINDING()
  ATTR(bool, RaycastTarget) {
    if (value.has_value()) raycast_target_ = *value;
    return raycast_target_;
  }
  URGE_BINDING()
  ATTR(bool, BlocksRaycasts) {
    if (value.has_value()) blocks_raycasts_ = *value;
    return blocks_raycasts_;
  }
  URGE_BINDING()
  ATTR(bool, Focusable) {
    if (value.has_value()) focusable_ = *value;
    return focusable_;
  }
  URGE_BINDING()
  ATTR(bool, Modal) {
    if (value.has_value()) modal_ = *value;
    return modal_;
  }
  URGE_BINDING()
  ATTR(bool, ClipChildren) {
    if (value.has_value()) clip_children_ = *value;
    return clip_children_;
  }
  URGE_BINDING()
  ATTR(bool, DoubleSided) {
    if (value.has_value()) double_sided_ = *value;
    return double_sided_;
  }
  URGE_BINDING()
  ATTR(float, Opacity) {
    if (value.has_value()) opacity_ = *value;
    return opacity_;
  }
  URGE_BINDING()
  ATTR(std::string, Text) {
    if (value.has_value()) text_ = *value;
    return text_;
  }
  URGE_BINDING()
  ATTR(std::string, Placeholder) {
    if (value.has_value()) placeholder_ = *value;
    return placeholder_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, Bitmap) {
    if (value.has_value()) bitmap_ = *value;
    return bitmap_;
  }
  URGE_BINDING()
  ATTR(int32_t, ImageMode) {
    if (value.has_value()) image_mode_ = *value;
    return image_mode_;
  }
  URGE_BINDING()
  ATTR(int32_t, Alignment) {
    if (value.has_value()) alignment_ = *value;
    return alignment_;
  }
  URGE_BINDING()
  ATTR(int32_t, VerticalAlignment) {
    if (value.has_value()) vertical_alignment_ = *value;
    return vertical_alignment_;
  }
  URGE_BINDING()
  ATTR(bool, WordWrap) {
    if (value.has_value()) word_wrap_ = *value;
    return word_wrap_;
  }
  URGE_BINDING()
  ATTR(bool, Checked) {
    if (value.has_value()) checked_ = *value;
    return checked_;
  }
  URGE_BINDING()
  ATTR(float, Value) {
    if (value.has_value()) {
      value_ = *value;
      ApplyScrollValue();
    }
    return value_;
  }
  URGE_BINDING()
  ATTR(float, ThumbSize) {
    if (value.has_value()) thumb_size_ = std::clamp(*value, 0.0f, 1.0f);
    return thumb_size_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Node>, ScrollTarget) {
    if (value.has_value()) scroll_target_ = *value;
    return scroll_target_;
  }
  URGE_BINDING()
  ATTR(float, MinValue) {
    if (value.has_value()) min_value_ = *value;
    return min_value_;
  }
  URGE_BINDING()
  ATTR(float, MaxValue) {
    if (value.has_value()) max_value_ = *value;
    return max_value_;
  }
  URGE_BINDING()
  ATTR(float, Step) {
    if (value.has_value()) step_ = *value;
    return step_;
  }
  URGE_BINDING()
  ATTR(bool, Vertical) {
    if (value.has_value()) vertical_ = *value;
    return vertical_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, Scroll) {
    if (value.has_value()) scroll_ = *value;
    return scroll_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, ContentSize) {
    if (value.has_value()) content_size_ = *value;
    return content_size_;
  }
  URGE_BINDING()
  ATTR(float, ScrollSpeed) {
    if (value.has_value()) scroll_speed_ = *value;
    return scroll_speed_;
  }
  URGE_BINDING()
  ATTR(bool, Multiline) {
    if (value.has_value()) multiline_ = *value;
    return multiline_;
  }
  URGE_BINDING()
  ATTR(bool, Password) {
    if (value.has_value()) password_ = *value;
    return password_;
  }
  URGE_BINDING()
  ATTR(bool, ReadOnly) {
    if (value.has_value()) read_only_ = *value;
    return read_only_;
  }
  URGE_BINDING()
  ATTR(bool, Numeric) {
    if (value.has_value()) numeric_ = *value;
    return numeric_;
  }
  URGE_BINDING()
  ATTR(int32_t, CharacterLimit) {
    if (value.has_value()) character_limit_ = *value;
    return character_limit_;
  }
  URGE_BINDING()
  ATTR(int32_t, LayoutMode) {
    if (value.has_value()) layout_mode_ = *value;
    return layout_mode_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, Spacing) {
    if (value.has_value()) spacing_ = *value;
    return spacing_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector4>, Padding) {
    if (value.has_value()) padding_ = *value;
    return padding_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, CellSize) {
    if (value.has_value()) cell_size_ = *value;
    return cell_size_;
  }
  URGE_BINDING()
  ATTR(int32_t, Columns) {
    if (value.has_value()) columns_ = *value;
    return columns_;
  }
  URGE_BINDING()
  ATTR(std::vector<std::string>, Items) {
    if (value.has_value()) items_ = *value;
    return items_;
  }
  URGE_BINDING()
  ATTR(int32_t, SelectedIndex) {
    if (value.has_value()) selected_index_ = *value;
    return selected_index_;
  }
  URGE_BINDING()
  ATTR(float, RowHeight) {
    if (value.has_value()) row_height_ = *value;
    return row_height_;
  }
  URGE_BINDING()
  ATTR(bool, Expanded) {
    if (value.has_value()) expanded_ = *value;
    return expanded_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, ReferenceSize) {
    if (value.has_value()) reference_size_ = *value;
    return reference_size_;
  }
  URGE_BINDING()
  ATTR(int32_t, ScaleMode) {
    if (value.has_value()) scale_mode_ = *value;
    return scale_mode_;
  }
  URGE_BINDING()
  ATTR(float, MatchWidthOrHeight) {
    if (value.has_value()) match_width_or_height_ = *value;
    return match_width_or_height_;
  }
  URGE_BINDING()
  int64_t Id() const { return id_; }
  URGE_BINDING()
  bool Hovered() const { return hovered; }
  URGE_BINDING()
  bool Focused() const { return focused; }
  URGE_BINDING()
  bool Pressed() const { return pressed; }
  URGE_BINDING()
  ATTR(int64_t, CursorPosition) {
    if (value) caret = static_cast<size_t>(std::max<int64_t>(0, *value));
    return static_cast<int64_t>(caret);
  }
  URGE_BINDING()
  ATTR(int64_t, SelectionPosition) {
    if (value) selection = static_cast<size_t>(std::max<int64_t>(0, *value));
    return static_cast<int64_t>(selection);
  }
  URGE_BINDING()
  RefPtr<Vector2> ContentExtent() const {
    return MakeRefCounted<Vector2>(content_extent);
  }
  URGE_BINDING()
  void Refresh() { paint_key = 0; }
  glm::vec4 bounds() const;
  glm::vec4 hit_bounds() const;
  bool Prepare(DrawParam param);
  bool Draw(DrawParam param);
  void Detach();
  void Paint(URGE_EXCEPT, int width, int height);
  std::vector<glm::vec3> clip_planes(const glm::vec2& target_size,
                                     bool* valid) const;
  UIKind kind = UIKind::Panel;
  glm::vec2 target_size{640, 480}, canvas_scale{1, 1};
  glm::vec2 content_extent{0};
  Node* owner = nullptr;
  bool hovered = false, focused = false, pressed = false;
  size_t caret = 0, selection = 0;
  float text_scroll = 0;
  std::string composition;
  uint64_t paint_key = 0;
  RefPtr<Bitmap> surface;
  PrimitiveEmitter::Slot slot{};
  float effective_opacity() const;
  bool accepts_input() const;
  glm::vec2 ScrollThumb() const;
  void ApplyScrollValue();
  RefPtr<UIStyle> style_ = MakeRefCounted<UIStyle>();
  RefPtr<UIRectTransform> rect_transform_ = MakeRefCounted<UIRectTransform>();
  bool interactable_ = true;
  bool raycast_target_ = true;
  bool blocks_raycasts_ = true;
  bool focusable_ = false;
  bool modal_ = false;
  bool clip_children_ = false;
  bool double_sided_ = true;
  float opacity_ = 1.0f;
  std::string text_ = "";
  std::string placeholder_ = "";
  MaybePtr<Bitmap> bitmap_ = nullptr;
  int32_t image_mode_ = 0;
  int32_t alignment_ = 0;
  int32_t vertical_alignment_ = 1;
  bool word_wrap_ = false;
  bool checked_ = false;
  float value_ = 0.0f;
  float min_value_ = 0.0f;
  float max_value_ = 1.0f;
  float step_ = 0.0f;
  bool vertical_ = false;
  float thumb_size_ = 0.2f;
  MaybePtr<Node> scroll_target_ = nullptr;
  RefPtr<Vector2> scroll_ = MakeRefCounted<Vector2>(0, 0);
  RefPtr<Vector2> content_size_ = MakeRefCounted<Vector2>(0, 0);
  float scroll_speed_ = 32.0f;
  bool multiline_ = false;
  bool password_ = false;
  bool read_only_ = false;
  bool numeric_ = false;
  int32_t character_limit_ = 0;
  int32_t layout_mode_ = 0;
  RefPtr<Vector2> spacing_ = MakeRefCounted<Vector2>(8, 8);
  RefPtr<Vector4> padding_ = MakeRefCounted<Vector4>(8, 8, 8, 8);
  RefPtr<Vector2> cell_size_ = MakeRefCounted<Vector2>(120, 40);
  int32_t columns_ = 1;
  std::vector<std::string> items_ = {};
  int32_t selected_index_ = -1;
  float row_height_ = 32.0f;
  bool expanded_ = false;
  RefPtr<Vector2> reference_size_ = MakeRefCounted<Vector2>(0, 0);
  int32_t scale_mode_ = 0;
  float match_width_or_height_ = 0.5f;

 private:
  int64_t id_;
};
glm::mat4 UIViewProjection(const Node* node, const glm::vec2& target_size);
}  // namespace urge
