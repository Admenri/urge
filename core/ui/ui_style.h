// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once

#include "core/canvas/bitmap.h"
#include "core/font/font.h"
namespace urge {
URGE_BINDING()
class UIStyle : public Object {
 public:
  URGE_BINDING()
  UIStyle() = default;
  URGE_BINDING()
  ATTR(RefPtr<Color>, Background) {
    if (value.has_value()) background_ = *value;
    return background_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, Foreground) {
    if (value.has_value()) foreground_ = *value;
    return foreground_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, HoverColor) {
    if (value.has_value()) hover_color_ = *value;
    return hover_color_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, PressedColor) {
    if (value.has_value()) pressed_color_ = *value;
    return pressed_color_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, DisabledColor) {
    if (value.has_value()) disabled_color_ = *value;
    return disabled_color_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, Accent) {
    if (value.has_value()) accent_ = *value;
    return accent_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, SelectionColor) {
    if (value.has_value()) selection_color_ = *value;
    return selection_color_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Color>, BorderColor) {
    if (value.has_value()) border_color_ = *value;
    return border_color_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, Skin) {
    if (value.has_value()) skin_ = *value;
    return skin_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, HoverSkin) {
    if (value.has_value()) hover_skin_ = *value;
    return hover_skin_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, PressedSkin) {
    if (value.has_value()) pressed_skin_ = *value;
    return pressed_skin_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, DisabledSkin) {
    if (value.has_value()) disabled_skin_ = *value;
    return disabled_skin_;
  }
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, FocusedSkin) {
    if (value.has_value()) focused_skin_ = *value;
    return focused_skin_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector4>, Slice) {
    if (value.has_value()) slice_ = *value;
    return slice_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Font>, Font) {
    if (value.has_value()) font_ = *value;
    return font_;
  }
  URGE_BINDING()
  ATTR(int32_t, BorderWidth) {
    if (value.has_value()) border_width_ = *value;
    return border_width_;
  }
  URGE_BINDING()
  ATTR(int32_t, Padding) {
    if (value.has_value()) padding_ = *value;
    return padding_;
  }
  URGE_BINDING()
  ATTR(int32_t, LineHeight) {
    if (value.has_value()) line_height_ = *value;
    return line_height_;
  }
  RefPtr<Color> background_ = MakeRefCounted<Color>(40, 46, 58, 240);
  RefPtr<Color> foreground_ = MakeRefCounted<Color>(235, 239, 247, 255);
  RefPtr<Color> hover_color_ = MakeRefCounted<Color>(58, 76, 99, 255);
  RefPtr<Color> pressed_color_ = MakeRefCounted<Color>(32, 110, 148, 255);
  RefPtr<Color> disabled_color_ = MakeRefCounted<Color>(76, 78, 85, 150);
  RefPtr<Color> accent_ = MakeRefCounted<Color>(62, 165, 211, 255);
  RefPtr<Color> selection_color_ = MakeRefCounted<Color>(32, 110, 148, 255);
  RefPtr<Color> border_color_ = MakeRefCounted<Color>(96, 110, 130, 255);
  MaybePtr<Bitmap> skin_ = nullptr;
  MaybePtr<Bitmap> hover_skin_ = nullptr;
  MaybePtr<Bitmap> pressed_skin_ = nullptr;
  MaybePtr<Bitmap> disabled_skin_ = nullptr;
  MaybePtr<Bitmap> focused_skin_ = nullptr;
  RefPtr<Vector4> slice_ = MakeRefCounted<Vector4>(8, 8, 8, 8);
  RefPtr<Font> font_ = MakeRefCounted<Font>(std::vector<std::string>{}, 20);
  int32_t border_width_ = 1;
  int32_t padding_ = 8;
  int32_t line_height_ = 0;
};
}  // namespace urge
