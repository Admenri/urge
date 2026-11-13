// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_state.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>

#include "SDL3/SDL_timer.h"
#include "core/common/logger.h"
#include "core/graphics/graphics.h"
#include "core/scene/sprite_batch.h"
#include "core/scene/viewport.h"
#include "core/ui/ui.h"
#include "core/ui/ui_text_util.h"
#include "glm/gtc/matrix_transform.hpp"

namespace urge {
namespace {
std::atomic<int64_t> next_id{1};
template <class T>
void Hash(uint64_t& key, const T& value) {
  key ^= std::hash<T>{}(value) + 0x9e3779b9 + (key << 6) + (key >> 2);
}
void HashColor(uint64_t& key, const RefPtr<Color>& color) {
  const auto value = color->Normalize();
  for (int i = 0; i < 4; ++i) Hash(key, value[i]);
}
Bitmap* StateSkin(const UIState* state) {
  const auto& style = state->style_;
  const auto& selected = !state->accepts_input() && state->focusable_
                             ? style->disabled_skin_
                         : state->pressed ? style->pressed_skin_
                         : state->hovered ? style->hover_skin_
                         : state->focused ? style->focused_skin_
                                          : style->skin_;
  if (Disposable::Check(selected)) return selected.get();
  return Disposable::Check(style->skin_) ? style->skin_.get() : nullptr;
}
void Fill(Bitmap* bitmap, ExceptionState& ex, int x, int y, int width,
          int height, const RefPtr<Color>& color) {
  if (width > 0 && height > 0) bitmap->FillRect(ex, x, y, width, height, color);
}
int TextWidth(Font* font, const std::string& text) {
  int w = 0, h = 0;
  font->MeasureText(text, &w, &h);
  return w;
}
std::vector<std::string> Lines(Font* font, const std::string& text, int width,
                               bool wrap) {
  std::vector<std::string> lines;
  std::string line;
  for (size_t i = 0; i < text.size();) {
    const size_t end = ui_text::Next(text, i);
    const std::string c = text.substr(i, end - i);
    i = end;
    if (c == "\n") {
      lines.push_back(line);
      line.clear();
      continue;
    }
    if (c == "\r") continue;
    if (wrap && !line.empty() && TextWidth(font, line + c) > width) {
      const auto space = line.find_last_of(" \t");
      if (space != std::string::npos && space > 0) {
        lines.push_back(line.substr(0, space));
        line = line.substr(space + 1);
      } else {
        lines.push_back(line);
        line.clear();
      }
    }
    line += c;
  }
  lines.push_back(line);
  return lines;
}
void NineSlice(Bitmap* destination, ExceptionState& ex, Bitmap* source,
               const glm::vec4& borders, int width, int height) {
  int sw = source->Width(), sh = source->Height();
  int left = std::clamp(static_cast<int>(borders.x), 0, sw);
  int top = std::clamp(static_cast<int>(borders.y), 0, sh);
  int right = std::clamp(static_cast<int>(borders.z), 0, sw - left);
  int bottom = std::clamp(static_cast<int>(borders.w), 0, sh - top);
  const float sx = left + right > width
                       ? static_cast<float>(width) / std::max(1, left + right)
                       : 1;
  const float sy = top + bottom > height
                       ? static_cast<float>(height) / std::max(1, top + bottom)
                       : 1;
  int xs[4] = {0, left, sw - right, sw}, ys[4] = {0, top, sh - bottom, sh};
  int xd[4] = {0, static_cast<int>(left * sx),
               width - static_cast<int>(right * sx), width};
  int yd[4] = {0, static_cast<int>(top * sy),
               height - static_cast<int>(bottom * sy), height};
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) {
      if (xs[x + 1] <= xs[x] || ys[y + 1] <= ys[y] || xd[x + 1] <= xd[x] ||
          yd[y + 1] <= yd[y])
        continue;
      destination->StretchBlt(
          ex,
          MakeRefCounted<Rect>(xd[x], yd[y], xd[x + 1] - xd[x],
                               yd[y + 1] - yd[y]),
          source,
          MakeRefCounted<Rect>(xs[x], ys[y], xs[x + 1] - xs[x],
                               ys[y + 1] - ys[y]));
    }
}
}  // namespace

UIState::UIState() : UIState(UIKind::Panel, nullptr) {}
UIState::UIState(UIKind type, Node* node)
    : kind(type), owner(node), id_(next_id++) {
  style_->font_->outline_ = false;
  style_->font_->shadow_ = false;
  switch (kind) {
    case UIKind::Canvas:
      raycast_target_ = false;
      if (Graphics::TryGet())
        target_size = {Graphics::Get().Width(), Graphics::Get().Height()};
      rect_transform_->size_->data = target_size;
      break;
    case UIKind::Text:
    case UIKind::Image:
    case UIKind::ProgressBar:
    case UIKind::LayoutGroup:
    case UIKind::Separator:
      raycast_target_ = false;
      break;
    default:
      break;
  }
  focusable_ = kind == UIKind::Button || kind == UIKind::Toggle ||
               kind == UIKind::Slider || kind == UIKind::ScrollBar ||
               kind == UIKind::InputField || kind == UIKind::Dropdown ||
               kind == UIKind::List;
  word_wrap_ = kind == UIKind::Text;
  clip_children_ = kind == UIKind::ScrollPanel;
  if (kind == UIKind::Dialog) modal_ = true;
  if (kind == UIKind::List) rect_transform_->size_->data = {240, 180};
  if (kind == UIKind::InputField) rect_transform_->size_->data = {240, 40};
  if (kind == UIKind::Panel || kind == UIKind::ScrollPanel ||
      kind == UIKind::Dialog)
    rect_transform_->size_->data = {300, 200};
}
UIState::~UIState() { Detach(); }
void UIState::Detach() {
  Node* previous = owner;
  owner = nullptr;
  if (previous && UI::TryGet()) UI::Get().Forget(previous);
  scroll_target_ = nullptr;
  surface.reset();
}
glm::vec4 UIState::bounds() const { return rect_transform_->rect(); }
glm::vec4 UIState::hit_bounds() const {
  auto rect = bounds();
  if (kind == UIKind::Dropdown && expanded_)
    rect.w += std::min<size_t>(items_.size(), 8) * std::max(1.0f, row_height_);
  return rect;
}
glm::mat4 UIViewProjection(const Node* node, const glm::vec2& size) {
  const auto camera = node->effective_camera();
  return camera ? camera->projection(glm::ivec2(size)) * camera->view()
                : glm::ortho(0.0f, std::max(1.0f, size.x),
                             std::max(1.0f, size.y), 0.0f);
}
float UIState::effective_opacity() const {
  float result = std::clamp(opacity_, 0.0f, 1.0f);
  for (Node* parent = owner ? owner->scene_parent() : nullptr; parent;
       parent = parent->scene_parent())
    if (auto state = parent->ui_state())
      result *= std::clamp(state->opacity_, 0.0f, 1.0f);
  return result;
}
bool UIState::accepts_input() const {
  if (!owner || !owner->visible_in_tree() || !interactable_) return false;
  for (Node* parent = owner->scene_parent(); parent;
       parent = parent->scene_parent())
    if (auto state = parent->ui_state(); state && !state->interactable_)
      return false;
  return true;
}
glm::vec2 UIState::ScrollThumb() const {
  const auto rect = bounds();
  const float pad = std::clamp(static_cast<float>(style_->padding_), 0.0f,
                               std::min(rect.z, rect.w) / 2);
  const float track = std::max(1.0f, (vertical_ ? rect.w : rect.z) - 2 * pad);
  const float length =
      std::clamp(track * thumb_size_, std::min(12.0f, track), track);
  const float ratio =
      max_value_ > min_value_
          ? std::clamp((value_ - min_value_) / (max_value_ - min_value_), 0.0f,
                       1.0f)
          : 0;
  return {pad + (track - length) * ratio, length};
}
void UIState::ApplyScrollValue() {
  if (kind != UIKind::ScrollBar || !Disposable::Check(scroll_target_)) return;
  auto target = scroll_target_->ui_state();
  if (!target) return;
  const int axis = vertical_ ? 1 : 0;
  const float extent =
      std::max(0.0f, target->content_extent[axis] -
                         target->rect_transform_->resolved_size[axis]);
  const float ratio =
      max_value_ > min_value_
          ? std::clamp((value_ - min_value_) / (max_value_ - min_value_), 0.0f,
                       1.0f)
          : 0;
  target->scroll_->data[axis] = extent * ratio;
}
std::vector<glm::vec3> UIState::clip_planes(const glm::vec2& size,
                                            bool* valid) const {
  std::vector<glm::vec3> planes;
  *valid = true;
  for (Node* parent = owner ? owner->scene_parent() : nullptr; parent;
       parent = parent->scene_parent()) {
    if (auto state = parent->ui_state(); state && state->clip_children_) {
      if (!ui_math::MaskPlanes(
              UIViewProjection(parent, size) * parent->resolved_world_matrix(),
              state->bounds(), &planes)) {
        *valid = false;
        break;
      }
    }
    // Legacy Viewport uses a screen-aligned scissor, not a transformed mask.
    if (auto viewport = parent->TryCast<Viewport>()) {
      const auto r = (*viewport->Attr_Rect())->data;
      const auto matrix = parent->resolved_world_matrix();
      const glm::vec2 position(matrix[3]);
      const glm::vec4 scissor(
          position.x + *viewport->Attr_OX(), position.y + *viewport->Attr_OY(),
          static_cast<float>(r.width), static_cast<float>(r.height));
      const auto projection = glm::ortho(0.0f, size.x, size.y, 0.0f);
      if (!ui_math::MaskPlanes(projection, scissor, &planes)) {
        *valid = false;
        break;
      }
    }
  }
  return planes;
}

bool UIState::Prepare(DrawParam param) {
  slot = {};
  target_size = glm::vec2(param->target->size());
  if (!owner || kind == UIKind::Canvas || kind == UIKind::LayoutGroup)
    return false;
  const auto rect = hit_bounds();
  if (rect.z <= 0 || rect.w <= 0 || effective_opacity() <= 0) return false;
  const int width = std::clamp(static_cast<int>(std::ceil(rect.z)), 1, 8192);
  const int height = std::clamp(static_cast<int>(std::ceil(rect.w)), 1, 8192);
  const bool direct_image =
      kind == UIKind::Image && image_mode_ == 0 && Disposable::Check(bitmap_);
  if (direct_image) {
    surface = bitmap_;
    paint_key = 0;
  } else {
    uint64_t key = 0x837ce21;
    Hash(key, width);
    Hash(key, height);
    Hash(key, text_);
    Hash(key, placeholder_);
    Hash(key, value_);
    Hash(key, min_value_);
    Hash(key, max_value_);
    Hash(key, checked_);
    Hash(key, hovered);
    Hash(key, pressed);
    Hash(key, focused);
    Hash(key, interactable_);
    Hash(key, accepts_input());
    Hash(key, expanded_);
    Hash(key, selected_index_);
    Hash(key, row_height_);
    Hash(key, scroll_->data.x);
    Hash(key, scroll_->data.y);
    Hash(key, vertical_);
    Hash(key, thumb_size_);
    Hash(key, alignment_);
    Hash(key, vertical_alignment_);
    Hash(key, word_wrap_);
    Hash(key, multiline_);
    Hash(key, password_);
    Hash(key, caret);
    Hash(key, selection);
    Hash(key, composition);
    if (focused && kind == UIKind::InputField)
      Hash(key, (SDL_GetTicks() / 500) % 2);
    HashColor(key, style_->background_);
    HashColor(key, style_->foreground_);
    HashColor(key, style_->hover_color_);
    HashColor(key, style_->pressed_color_);
    HashColor(key, style_->disabled_color_);
    HashColor(key, style_->accent_);
    HashColor(key, style_->selection_color_);
    HashColor(key, style_->border_color_);
    Hash(key, style_->border_width_);
    Hash(key, style_->padding_);
    Hash(key, style_->line_height_);
    Hash(key, style_->font_->size_);
    Hash(key, style_->font_->bold_);
    Hash(key, style_->font_->italic_);
    Hash(key, style_->font_->outline_);
    Hash(key, style_->font_->shadow_);
    Hash(key, style_->font_->solid_);
    HashColor(key, style_->font_->out_color_);
    HashColor(key, style_->font_->gradient_color_);
    for (const auto& name : style_->font_->name_) Hash(key, name);
    for (const auto& item : items_) Hash(key, item);
    Hash(key, reinterpret_cast<uintptr_t>(bitmap_.get()));
    Hash(key, image_mode_);
    for (const auto& skin :
         {style_->skin_, style_->hover_skin_, style_->pressed_skin_,
          style_->disabled_skin_, style_->focused_skin_}) {
      Hash(key, reinterpret_cast<uintptr_t>(skin.get()));
      if (skin) {
        Hash(key, skin->content_serial());
        Hash(key, skin->IsDisposed());
      }
    }
    for (int i = 0; i < 4; ++i) Hash(key, style_->slice_->data[i]);
    if (bitmap_) Hash(key, bitmap_->content_serial());
    if (key != paint_key || !Disposable::Check(surface)) {
      ExceptionState ex;
      surface = MakeRefCounted<Bitmap>(ex, width, height);
      if (!ex.HadException())
        Paint(ex, width, height);
      if (ex.HadException()) {
        LOGGER_ERROR("[UI] {}", ex.message());
        surface.reset();
        return false;
      }
      paint_key = key;
    }
  }
  bool valid;
  const auto planes = clip_planes(target_size, &valid);
  if (!valid || !Disposable::Check(surface)) return false;
  const auto model = owner->world_transform();
  const auto vertices = ui_math::ClipQuad(
      rect, UIViewProjection(owner, target_size) * model, planes);
  if (vertices.size() < 3) return false;
  auto& emitter = SpriteBatch::Get().emitter();
  emitter.BeginTriangle().Color4f(1, 1, 1, effective_opacity());
  for (size_t i = 1; i + 1 < vertices.size(); ++i)
    for (const auto& v : {vertices[0], vertices[i], vertices[i + 1]})
      emitter.Texcoord2f(v.uv).Vertex4f(glm::vec4(v.position, 1));
  // SpriteBatch indexes per-instance data by vertex_index / 6. A clipped
  // polygon may have an odd number of triangles; pad it with a degenerate
  // triangle so the next widget starts on a six-vertex boundary.
  if ((vertices.size() - 2) % 2)
    for (int i = 0; i < 3; ++i)
      emitter.Texcoord2f(vertices[0].uv)
          .Vertex4f(glm::vec4(vertices[0].position, 1));
  slot = emitter.End();
  SpriteParam data{};
  data.model_mat = model;
  data.blend_color = glm::vec4(0);
  data.blend_tone = glm::vec4(0);
  data.bush_opacity = 1;
  SpriteBatch::Get().SetParam(slot.first, slot.count, data);
  return slot.count != 0;
}
bool UIState::Draw(DrawParam param) {
  if (!slot.count || !surface) return false;
  FlushSpriteBatch(param);
  auto& run = SpriteBatch::Get().run();
  run.active = true;
  run.texture = surface;
  run.blend_type = 0;
  run.scene = param->scene;
  run.first_vertex = slot.first;
  run.end_vertex = slot.first + slot.count;
  FlushSpriteBatch(param);
  return false;
}

void UIState::Paint(URGE_EXCEPT, int width, int height) {
  Bitmap* output = surface.get();
  auto style = style_;
  auto font = MakeRefCounted<Font>(style->font_);
  font->color_ = style->foreground_;
  output->Attr_Font(font);
  const int pad = std::clamp(style->padding_, 0, std::min(width, height) / 2);
  const int border =
      std::clamp(style->border_width_, 0, std::min(width, height) / 2);
  const int line = style->line_height_ > 0 ? style->line_height_
                                           : std::max(1, font->size_ + 6);
  auto text = [&](const std::string& value, int x, int y, int w, int h,
                  int align = 0) {
    if (w > 0 && h > 0)
      output->DrawText(exception_state, x, y, w, h, value,
                       std::clamp(align, 0, 2));
  };
  auto block = [&](const std::string& value, int x, int y, int w, int h,
                   bool wrap, int align, int valign) {
    const auto lines = Lines(font.get(), value, std::max(1, w), wrap);
    int start = y;
    if (valign == 1)
      start += std::max(0, (h - static_cast<int>(lines.size()) * line) / 2);
    else if (valign == 2)
      start += std::max(0, h - static_cast<int>(lines.size()) * line);
    for (const auto& item : lines) {
      text(item, x, start, w, line, align);
      start += line;
      if (start >= y + h) break;
    }
  };
  const bool plain = kind == UIKind::Text || kind == UIKind::Image ||
                     kind == UIKind::Separator;
  RefPtr<Color> background = !accepts_input() && focusable_
                                 ? style->disabled_color_
                             : pressed ? style->pressed_color_
                             : hovered ? style->hover_color_
                                       : style->background_;
  if (!plain) {
    if (auto* skin = StateSkin(this))
      NineSlice(output, exception_state, skin, style->slice_->data, width,
                height);
    else
      Fill(output, exception_state, 0, 0, width, height, background);
    const auto color = focused ? style->accent_ : style->border_color_;
    Fill(output, exception_state, 0, 0, width, border, color);
    Fill(output, exception_state, 0, height - border, width, border, color);
    Fill(output, exception_state, 0, 0, border, height, color);
    Fill(output, exception_state, width - border, 0, border, height, color);
  }
  if (kind == UIKind::Image) {
    if (Disposable::Check(bitmap_)) {
      if (image_mode_ == 2)
        NineSlice(output, exception_state, bitmap_.get(), style->slice_->data,
                  width, height);
      else {
        float scale = std::min(
            static_cast<float>(width) / std::max(1, bitmap_->Width()),
            static_cast<float>(height) / std::max(1, bitmap_->Height()));
        int w = std::max(1, static_cast<int>(bitmap_->Width() * scale));
        int h = std::max(1, static_cast<int>(bitmap_->Height() * scale));
        output->StretchBlt(
            exception_state,
            MakeRefCounted<Rect>((width - w) / 2, (height - h) / 2, w, h),
            bitmap_, bitmap_->GetRect());
      }
    }
    return;
  }
  if (kind == UIKind::Separator) {
    Fill(output, exception_state, 0, 0, width, height, style->foreground_);
    return;
  }
  const float ratio =
      max_value_ > min_value_
          ? std::clamp((value_ - min_value_) / (max_value_ - min_value_), 0.0f,
                       1.0f)
          : 0;
  if (kind == UIKind::ScrollBar) {
    const auto thumb = ScrollThumb();
    if (vertical_)
      Fill(output, exception_state, pad, static_cast<int>(thumb.x),
           width - 2 * pad, static_cast<int>(thumb.y), style->accent_);
    else
      Fill(output, exception_state, static_cast<int>(thumb.x), pad,
           static_cast<int>(thumb.y), height - 2 * pad, style->accent_);
    return;
  }
  if (kind == UIKind::ProgressBar || kind == UIKind::Slider) {
    if (vertical_)
      Fill(output, exception_state, pad,
           pad + static_cast<int>((height - 2 * pad) * (1 - ratio)),
           width - 2 * pad, static_cast<int>((height - 2 * pad) * ratio),
           style->accent_);
    else
      Fill(output, exception_state, pad, pad,
           static_cast<int>((width - 2 * pad) * ratio), height - 2 * pad,
           style->accent_);
    if (kind != UIKind::ProgressBar) {
      if (vertical_)
        Fill(output, exception_state, pad,
             std::clamp(
                 pad + static_cast<int>((height - 2 * pad) * (1 - ratio)) - 3,
                 0, std::max(0, height - 6)),
             width - 2 * pad, std::min(height, 6), style->foreground_);
      else
        Fill(output, exception_state,
             std::clamp(pad + static_cast<int>((width - 2 * pad) * ratio) - 3,
                        0, std::max(0, width - 6)),
             pad, std::min(width, 6), height - 2 * pad, style->foreground_);
    }
  }
  if (kind == UIKind::Toggle) {
    const int side = std::max(1, std::min(height - 2 * pad, 24));
    Fill(output, exception_state, pad, (height - side) / 2, side, side,
         style->border_color_);
    if (checked_)
      Fill(output, exception_state, pad + 3, (height - side) / 2 + 3,
           std::max(0, side - 6), std::max(0, side - 6), style->accent_);
    text(text_, pad + side + pad, 0, width - side - 3 * pad, height);
    return;
  }
  if (kind == UIKind::Dropdown) {
    const int base = std::max(1, static_cast<int>(bounds().w));
    const std::string title =
        selected_index_ >= 0 &&
                static_cast<size_t>(selected_index_) < items_.size()
            ? items_[selected_index_]
            : text_;
    text(title, pad, 0, width - 3 * pad - 12, base, alignment_);
    text(expanded_ ? "-" : "+", width - 2 * pad - 12, 0, 12, base);
    if (expanded_) {
      const int row = std::max(1, static_cast<int>(row_height_));
      const int first = std::max(0, static_cast<int>(scroll_->data.y / row));
      for (int index = first; index < std::min<int>(items_.size(), first + 8);
           ++index) {
        const int y = base + (index - first) * row;
        if (index == selected_index_)
          Fill(output, exception_state, border, y, width - 2 * border, row,
               style->hover_color_);
        text(items_[index], pad, y, width - 2 * pad, row);
      }
    }
    return;
  }
  if (kind == UIKind::List) {
    const int row = std::max(1, static_cast<int>(row_height_));
    const int first = std::max(0, static_cast<int>(scroll_->data.y / row));
    for (int index = first; index < static_cast<int>(items_.size()); ++index) {
      const int y = index * row - static_cast<int>(scroll_->data.y);
      if (y >= height) break;
      if (index == selected_index_)
        Fill(output, exception_state, border, y, width - 2 * border, row,
             style->hover_color_);
      text(items_[index], pad, y, width - 2 * pad, row);
    }
    return;
  }
  if (kind == UIKind::InputField) {
    caret = ui_text::Boundary(text_, caret);
    selection = ui_text::Boundary(text_, selection);
    const bool empty = text_.empty() && composition.empty();
    std::string display = text_;
    if (password_) {
      display.clear();
      for (size_t i = 0; i < text_.size(); i = ui_text::Next(text_, i))
        display += text_[i] == '\n' ? '\n' : '*';
    }
    size_t cursor = password_ ? ui_text::Count(text_.substr(0, caret)) : caret;
    size_t anchor =
        password_ ? ui_text::Count(text_.substr(0, selection)) : selection;
    const int inner = std::max(1, width - 2 * pad);
    if (!multiline_) {
      const int cx = TextWidth(font.get(), display.substr(0, cursor));
      text_scroll =
          std::clamp(text_scroll, 0.0f,
                     static_cast<float>(std::max(
                         0, TextWidth(font.get(), display) - inner + 2)));
      if (cx - text_scroll > inner - 2) text_scroll = cx - inner + 2;
      if (cx < text_scroll) text_scroll = static_cast<float>(cx);
      if (cursor != anchor) {
        const int a =
            TextWidth(font.get(), display.substr(0, std::min(cursor, anchor)));
        const int b =
            TextWidth(font.get(), display.substr(0, std::max(cursor, anchor)));
        Fill(output, exception_state, pad + a - static_cast<int>(text_scroll),
             (height - line) / 2, b - a, line, style->selection_color_);
      }
      text(empty ? placeholder_ : display, pad - static_cast<int>(text_scroll),
           0, std::max(inner, TextWidth(font.get(), display) + 4), height);
      if (!composition.empty())
        text(composition, pad + cx - static_cast<int>(text_scroll), 0,
             std::max(inner, TextWidth(font.get(), composition) + 4), height);
      if (focused && (SDL_GetTicks() / 500) % 2 == 0)
        Fill(output, exception_state, pad + cx - static_cast<int>(text_scroll),
             (height - line) / 2, 1, line, style->foreground_);
    } else {
      const auto lines = Lines(font.get(), display, inner, false);
      const int current_line = static_cast<int>(
          std::count(display.begin(),
                     display.begin() + std::min(cursor, display.size()), '\n'));
      const size_t start =
          cursor == 0 ? std::string::npos : display.rfind('\n', cursor - 1);
      const int cx =
          TextWidth(font.get(),
                    display.substr(
                        start == std::string::npos ? 0 : start + 1,
                        cursor - (start == std::string::npos ? 0 : start + 1)));
      if (current_line * line - scroll_->data.y > height - 2 * pad - line)
        scroll_->data.y =
            static_cast<float>(current_line * line - (height - 2 * pad - line));
      if (current_line * line < scroll_->data.y)
        scroll_->data.y = static_cast<float>(current_line * line);
      int y = pad - static_cast<int>(scroll_->data.y);
      size_t offset = 0;
      for (const auto& item : lines) {
        const size_t a =
            std::clamp(std::min(cursor, anchor), offset, offset + item.size());
        const size_t b =
            std::clamp(std::max(cursor, anchor), offset, offset + item.size());
        const size_t row_end = offset + item.size() +
                               (offset + item.size() < display.size() ? 1 : 0);
        if (std::min(cursor, anchor) < row_end &&
            std::max(cursor, anchor) > offset) {
          int x = TextWidth(font.get(), item.substr(0, a - offset));
          int right = TextWidth(font.get(), item.substr(0, b - offset));
          if (b < std::max(cursor, anchor)) right = std::max(right, inner);
          Fill(output, exception_state, pad + x, y, std::max(0, right - x),
               line, style->selection_color_);
        }
        text(item, pad, y, std::max(inner, TextWidth(font.get(), item) + 4),
             line);
        y += line;
        offset += item.size() + 1;
      }
      if (empty)
        block(placeholder_, pad, pad, inner, height - 2 * pad, true, alignment_,
              0);
      if (!composition.empty())
        text(composition, pad + cx,
             pad + current_line * line - static_cast<int>(scroll_->data.y),
             std::max(inner, TextWidth(font.get(), composition) + 4), line);
      if (focused && (SDL_GetTicks() / 500) % 2 == 0)
        Fill(output, exception_state, pad + cx,
             pad + current_line * line - static_cast<int>(scroll_->data.y), 1,
             line, style->foreground_);
    }
    return;
  }
  block(text_, pad, pad, width - 2 * pad, height - 2 * pad, word_wrap_,
        alignment_, vertical_alignment_);
}
}  // namespace urge
