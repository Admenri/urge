// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_mouse.h"
#include "core/graphics/graphics.h"
#include "core/ui/ui_text_util.h"
#include "core/ui/ui_widgets.h"
#include "glm/gtc/matrix_transform.hpp"

namespace urge {
namespace {
bool Descendant(Node* node, Node* root) {
  for (; node; node = node->scene_parent())
    if (node == root) return true;
  return false;
}
glm::vec2 LocalPoint(Node* node, glm::vec2 point) {
  ui_math::Ray ray;
  glm::vec3 local(0);
  float distance;
  auto state = node->ui_state();
  if (state) point = glm::clamp(point, glm::vec2(0), state->target_size);
  if (state &&
      ui_math::ScreenRay(point, state->target_size,
                         UIViewProjection(node, state->target_size), &ray))
    ui_math::Intersect(ray, node->resolved_world_matrix(),
                       glm::vec4(-1e8f, -1e8f, 2e8f, 2e8f), &local, &distance);
  return glm::vec2(local);
}
float Measure(UIState* state, const std::string& text) {
  int width = 0, height = 0;
  state->style_->font_->MeasureText(text, &width, &height);
  return static_cast<float>(width);
}
size_t InputCursor(UIState* state, glm::vec2 local) {
  const auto r = state->bounds();
  const int pad = state->style_->padding_;
  const float px =
      local.x - r.x - pad + (state->multiline_ ? 0 : state->text_scroll);
  size_t begin = 0, end = state->text_.size();
  if (state->multiline_) {
    const int line = state->style_->line_height_ > 0
                         ? state->style_->line_height_
                         : state->style_->font_->size_ + 6;
    int row = std::max(
        0, static_cast<int>((local.y - r.y - pad + state->scroll_->data.y) /
                            std::max(1, line)));
    while (row-- && begin < state->text_.size()) {
      auto next = state->text_.find('\n', begin);
      begin = next == std::string::npos ? state->text_.size() : next + 1;
    }
    end = state->text_.find('\n', begin);
    if (end == std::string::npos) end = state->text_.size();
  }
  size_t cursor = begin;
  while (cursor < end) {
    const size_t next = ui_text::Next(state->text_, cursor);
    const float a =
        Measure(state, state->password_
                           ? std::string(ui_text::Count(state->text_.substr(
                                             begin, cursor - begin)),
                                         '*')
                           : state->text_.substr(begin, cursor - begin));
    const float b = Measure(
        state,
        state->password_
            ? std::string(
                  ui_text::Count(state->text_.substr(begin, next - begin)), '*')
            : state->text_.substr(begin, next - begin));
    if (px < (a + b) / 2) break;
    cursor = next;
  }
  return cursor;
}
void ClampScroll(UIState* state) {
  auto size = state->rect_transform_->resolved_size;
  if (state->kind == UIKind::Dropdown)
    size.y += std::min<size_t>(state->items_.size(), 8) *
              std::max(1.0f, state->row_height_);
  state->scroll_->data =
      glm::clamp(state->scroll_->data, glm::vec2(0),
                 glm::max(glm::vec2(0), state->content_extent - size));
  if (state->kind == UIKind::Dropdown) {
    const float row = std::max(1.0f, state->row_height_);
    state->scroll_->data.y = std::floor(state->scroll_->data.y / row) * row;
  }
}
void UpdateInputArea(Node* node) {
  if (!node || !Graphics::TryGet()) return;
  auto state = node->ui_state();
  if (!state || state->kind != UIKind::InputField) return;
  const auto bounds = state->bounds();
  const auto mvp = UIViewProjection(node, state->target_size) *
                   node->resolved_world_matrix();
  glm::vec2 minimum(1e9f), maximum(-1e9f);
  for (auto corner :
       {glm::vec2(bounds.x, bounds.y), glm::vec2(bounds.x + bounds.z, bounds.y),
        glm::vec2(bounds.x, bounds.y + bounds.w),
        glm::vec2(bounds.x + bounds.z, bounds.y + bounds.w)}) {
    auto projected = mvp * glm::vec4(corner, 0, 1);
    if (projected.w <= 1e-6f) return;
    projected /= projected.w;
    auto window = Graphics::Get().ScreenToWindow(
        (projected.x + 1) * state->target_size.x / 2,
        (1 - projected.y) * state->target_size.y / 2);
    minimum = glm::min(minimum, window);
    maximum = glm::max(maximum, window);
  }
  SDL_Rect area{
      static_cast<int>(minimum.x), static_cast<int>(minimum.y),
      std::max(1, static_cast<int>(std::ceil(maximum.x - minimum.x))),
      std::max(1, static_cast<int>(std::ceil(maximum.y - minimum.y)))};
  SDL_SetTextInputArea(Graphics::Get().window(), &area, 0);
}
}  // namespace

UI::~UI() {
  events_.clear();
  captures_.clear();
  hovered_.reset();
  focused_.reset();
}
void UI::RegisterCanvas(UICanvas* canvas) { canvases_.push_back(canvas); }
void UI::UnregisterCanvas(UICanvas* canvas) {
  canvases_.erase(std::remove(canvases_.begin(), canvases_.end(), canvas),
                  canvases_.end());
}
void UI::Forget(Node* node) {
  if (hovered_.get() == node) hovered_.reset();
  if (focused_.get() == node) {
    focused_.reset();
    if (Graphics::TryGet()) SDL_StopTextInput(Graphics::Get().window());
  }
  for (auto it = captures_.begin(); it != captures_.end();) {
    if (it->second.target.get() == node || it->second.drag_target.get() == node)
      it = captures_.erase(it);
    else
      ++it;
  }
}
void UI::PrepareTree(Node* root, const glm::vec2& size) {
  if (!root || root->IsDisposed() || size.x <= 0 || size.y <= 0) return;
  std::function<void(Node*, glm::vec4)> visit = [&](Node* node,
                                                    glm::vec4 parent_rect) {
    UIState* state = node->ui_state();
    glm::vec4 rect = parent_rect;
    if (state) {
      state->target_size = size;
      auto layout = state->rect_transform_;
      if (state->kind == UIKind::Canvas) {
        const auto reference = state->reference_size_->data;
        glm::vec2 extent(reference.x > 0 ? reference.x : size.x,
                         reference.y > 0 ? reference.y : size.y);
        state->canvas_scale = glm::vec2(1);
        if (state->scale_mode_ == 1)
          state->canvas_scale =
              glm::vec2(std::min(size.x / extent.x, size.y / extent.y));
        if (state->scale_mode_ == 2) {
          float match = std::clamp(state->match_width_or_height_, 0.0f, 1.0f);
          state->canvas_scale =
              glm::vec2(std::exp2(std::log2(size.x / extent.x) * (1 - match) +
                                  std::log2(size.y / extent.y) * match));
        }
        layout->resolved_size = extent;
        layout->resolved_position =
            state->scale_mode_ ? (size - extent * state->canvas_scale) * 0.5f
                               : glm::vec2(0);
      } else
        layout->Resolve(parent_rect);
      rect = state->bounds();
    }
    auto children = node->children_raw();
    for (Node* child : children)
      if (auto c = child->ui_state()) c->rect_transform_->ClearLayout();
    if (state && state->layout_mode_ != 0) {
      const auto padding = state->padding_->data;
      const auto spacing = state->spacing_->data;
      const glm::vec2 available =
          glm::max(glm::vec2(0), glm::vec2(rect.z - padding.x - padding.z,
                                           rect.w - padding.y - padding.w));
      std::vector<UIState*> items;
      for (Node* child : children)
        if (auto item = child->ui_state();
            item && child->visible_in_tree() &&
            !item->rect_transform_->ignore_layout_)
          items.push_back(item);
      float fixed = 0;
      int flexible = 0;
      const int axis = state->layout_mode_ == 2 ? 1 : 0;
      for (auto item : items) {
        const bool expand = axis ? item->rect_transform_->expand_height_
                                 : item->rect_transform_->expand_width_;
        if (expand)
          ++flexible;
        else
          fixed += std::max(0.0f, item->rect_transform_->size_->data[axis]);
      }
      const float share =
          flexible
              ? std::max(
                    0.0f,
                    available[axis] - fixed -
                        spacing[axis] *
                            std::max(0, static_cast<int>(items.size()) - 1)) /
                    flexible
              : 0;
      glm::vec2 cursor(rect.x + padding.x, rect.y + padding.y);
      int index = 0;
      for (auto item : items) {
        auto layout = item->rect_transform_;
        glm::vec2 extent = layout->size_->data;
        if (state->layout_mode_ == 3) {
          extent = state->cell_size_->data;
          int columns = std::max(1, state->columns_);
          cursor =
              glm::vec2(rect.x + padding.x, rect.y + padding.y) +
              glm::vec2(index % columns, index / columns) * (extent + spacing);
        } else {
          if (axis == 0 && layout->expand_width_) extent.x = share;
          if (axis == 1 && layout->expand_height_) extent.y = share;
          if (axis == 0 && layout->expand_height_) extent.y = available.y;
          if (axis == 1 && layout->expand_width_) extent.x = available.x;
        }
        layout->driven = true;
        layout->driven_size = extent;
        layout->driven_position = cursor + extent * layout->pivot_->data;
        cursor[axis] += extent[axis] + spacing[axis];
        ++index;
      }
    }
    for (Node* child : children) visit(child, rect);
    if (state) {
      glm::vec2 content = state->content_size_->data;
      if (state->kind == UIKind::List)
        content = {rect.z,
                   std::max(1.0f, state->row_height_) * state->items_.size()};
      else if (state->kind == UIKind::Dropdown)
        content = {
            rect.z,
            std::max(1.0f, state->row_height_) * state->items_.size() + rect.w};
      else if (state->kind == UIKind::ScrollPanel) {
        glm::vec2 measured(rect.z, rect.w);
        for (Node* child : children)
          if (auto c = child->ui_state()) {
            const auto bounds = c->bounds();
            const auto matrix = child->local_matrix();
            for (auto corner :
                 {glm::vec2(bounds.x, bounds.y),
                  glm::vec2(bounds.x + bounds.z, bounds.y),
                  glm::vec2(bounds.x, bounds.y + bounds.w),
                  glm::vec2(bounds.x + bounds.z, bounds.y + bounds.w)}) {
              const auto p = matrix * glm::vec4(corner, 0, 1);
              measured = glm::max(measured, glm::vec2(p) - glm::vec2(rect));
            }
          }
        if (content.x <= 0) content.x = measured.x;
        if (content.y <= 0) content.y = measured.y;
      }
      state->content_extent = content;
      if (state->kind == UIKind::ScrollPanel || state->kind == UIKind::List ||
          state->kind == UIKind::Dropdown)
        ClampScroll(state);
    }
  };
  visit(root, glm::vec4(0, 0, size));
  // Resolve bindings after all target content extents have been measured.
  std::function<void(Node*)> sync_scrollbars = [&](Node* node) {
    auto state = node->ui_state();
    if (state && state->kind == UIKind::ScrollBar && state->scroll_target_) {
      if (!Disposable::Check(state->scroll_target_))
        state->scroll_target_ = nullptr;
      else if (auto target = state->scroll_target_->ui_state()) {
        const int axis = state->vertical_ ? 1 : 0;
        const float viewport = target->rect_transform_->resolved_size[axis];
        const float content = target->content_extent[axis];
        const float extent = std::max(0.0f, content - viewport);
        state->thumb_size_ =
            content > 0 ? std::clamp(viewport / content, 0.0f, 1.0f) : 1;
        const float ratio =
            extent > 0
                ? std::clamp(target->scroll_->data[axis] / extent, 0.0f, 1.0f)
                : 0;
        state->value_ =
            state->min_value_ +
            ratio * std::max(0.0f, state->max_value_ - state->min_value_);
      }
    }
    for (Node* child : node->children_raw()) sync_scrollbars(child);
  };
  sync_scrollbars(root);
}
void UI::Update() {
  if (Graphics::TryGet())
    PrepareTree(Graphics::Get().root().get(),
                glm::vec2(Graphics::Get().Width(), Graphics::Get().Height()));
  Node* modal = Modal();
  if (focused_ && (!focused_->ui_state()->accepts_input() ||
                   (modal && !Descendant(focused_.get(), modal))))
    Focus(nullptr);
  UpdateInputArea(focused_.get());
  if (hovered_ && (!hovered_->ui_state() || !hovered_->visible_in_tree())) {
    if (hovered_->ui_state()) hovered_->ui_state()->hovered = false;
    hovered_.reset();
  }
  for (auto it = captures_.begin(); it != captures_.end();) {
    if (!it->second.target->ui_state() ||
        !it->second.target->ui_state()->accepts_input()) {
      auto target = it->second.target;
      it = captures_.erase(it);
      if (target->ui_state()) target->ui_state()->pressed = false;
      Emit("pointer_cancel", target.get());
    } else
      ++it;
  }
}
RefPtr<UIEvent> UI::PollEvent() {
  while (!events_.empty()) {
    auto event = events_.front();
    events_.pop_front();
    if (event->target && !event->target->IsDisposed()) return event;
  }
  return nullptr;
}
void UI::Emit(std::string type, Node* target, int64_t pointer,
              glm::vec2 position, glm::vec2 local) {
  if (!target || target->IsDisposed()) return;
  // Bound the queue when a native caller uses polling without draining it.
  if (events_.size() >= 4096) events_.pop_front();
  auto event =
      MakeRefCounted<UIEvent>(std::move(type), target, pointer, position);
  event->local_position = local;
  events_.push_back(event);
}
Node* UI::Modal() const {
  Node* result = nullptr;
  std::function<void(Node*)> visit = [&](Node* node) {
    if (!node->visible_in_tree()) return;
    if (auto state = node->ui_state(); state && state->modal_) result = node;
    for (auto child : node->children_raw()) visit(child);
  };
  if (Graphics::TryGet()) visit(Graphics::Get().root().get());
  return result;
}
RefPtr<UIRaycastHit> UI::Hit(glm::vec2 point) {
  RefPtr<UIRaycastHit> result;
  // Traverse the actual Node draw order, including intervening Sprite/Plane
  // parents and nested canvases. Canvas creation order is not a sort key.
  std::function<void(Node*)> visit = [&](Node* node) {
    if (!node->visible_in_tree()) return;
    if (auto state = node->ui_state(); state && state->kind == UIKind::Canvas) {
      if (auto hit = node->TryCast<UICanvas>()->Raycaster()->RaycastScreen(
              point.x, point.y))
        result = hit;
      return;
    }
    for (auto child : node->children_raw()) visit(child);
  };
  if (Graphics::TryGet()) visit(Graphics::Get().root().get());
  Node* modal = Modal();
  if (modal && (!result || !Descendant(result->target.get(), modal))) {
    result = MakeRefCounted<UIRaycastHit>();
    result->target = modal;
    result->local_position = glm::vec3(LocalPoint(modal, point), 0);
  }
  return result;
}
void UI::Focus(RefPtr<Node> node) {
  if (node && (!node->ui_state() || !node->ui_state()->accepts_input() ||
               !node->ui_state()->focusable_))
    return;
  if (node && Modal() && !Descendant(node.get(), Modal())) return;
  if (focused_ == node) return;
  if (focused_) {
    focused_->ui_state()->focused = false;
    focused_->ui_state()->composition.clear();
    Emit("blur", focused_.get());
  }
  focused_ = node;
  if (Graphics::TryGet()) SDL_StopTextInput(Graphics::Get().window());
  if (focused_) {
    auto state = focused_->ui_state();
    state->focused = true;
    state->caret = ui_text::Boundary(state->text_, state->caret);
    state->selection = ui_text::Boundary(state->text_, state->selection);
    Emit("focus", focused_.get());
    if (state->kind == UIKind::InputField && Graphics::TryGet()) {
      UpdateInputArea(node.get());
      SDL_StartTextInput(Graphics::Get().window());
    }
  }
}
void UI::CancelPointers() {
  auto captures = std::move(captures_);
  captures_.clear();
  touch_ids_.clear();
  for (auto& [id, capture] : captures) {
    if (capture.target->ui_state()) capture.target->ui_state()->pressed = false;
    Emit("pointer_cancel", capture.target.get(), id);
  }
  if (hovered_ && hovered_->ui_state()) hovered_->ui_state()->hovered = false;
  hovered_.reset();
}
bool UI::Pointer(std::string type, float x, float y, int64_t id) {
  Update();
  const glm::vec2 point(x, y);
  const auto hit = Hit(point);
  auto node = hit ? hit->target : RefPtr<Node>();
  const auto captured = captures_.find(id);
  if (type == "move" || type == "down") {
    if (id == 0 && hovered_ != node) {
      if (hovered_ && hovered_->ui_state()) {
        hovered_->ui_state()->hovered = false;
        Emit("pointer_exit", hovered_.get(), id, point);
      }
      hovered_ = node;
      if (hovered_ && hovered_->ui_state()) {
        hovered_->ui_state()->hovered = true;
        Emit("pointer_enter", hovered_.get(), id, point);
      }
    }
  }
  if (type == "down") {
    // Close dropdowns when clicking outside their header/popup.
    std::function<void(Node*)> close = [&](Node* candidate) {
      if (auto s = candidate->ui_state();
          s && s->kind == UIKind::Dropdown && candidate != node.get())
        s->expanded_ = false;
      for (auto child : candidate->children_raw()) close(child);
    };
    if (Graphics::TryGet()) close(Graphics::Get().root().get());
    if (!node) {
      Focus(nullptr);
      return false;
    }
    auto state = node->ui_state();
    if (!state || !state->accepts_input()) return true;
    if (state->focusable_)
      Focus(node);
    else if (!Modal())
      Focus(nullptr);
    Capture capture;
    capture.target = node;
    capture.start = point;
    capture.last = point;
    for (Node* ancestor = node.get(); ancestor;
         ancestor = ancestor->scene_parent())
      if (auto parent = ancestor->ui_state();
          parent && (parent->kind == UIKind::ScrollPanel ||
                     parent->kind == UIKind::List)) {
        capture.drag_target = ancestor;
        break;
      }
    captures_[id] = capture;
    state->pressed = true;
    const auto local = hit->local_position;
    Emit("pointer_down", node.get(), id, point, glm::vec2(local));
    if (state->kind == UIKind::ScrollBar) {
      const auto thumb = state->ScrollThumb();
      const auto rect = state->bounds();
      const float coordinate =
          state->vertical_ ? local.y - rect.y : local.x - rect.x;
      if (coordinate >= thumb.x && coordinate <= thumb.x + thumb.y)
        captures_[id].slider_offset = coordinate - (thumb.x + thumb.y / 2);
    }
    if (state->kind == UIKind::Slider || state->kind == UIKind::ScrollBar)
      SetSlider(node.get(), point, captures_[id].slider_offset);
    if (state->kind == UIKind::InputField)
      state->caret = state->selection = InputCursor(state, glm::vec2(local));
    return true;
  }
  if (type == "move" && captured != captures_.end()) {
    auto& capture = captured->second;
    auto state = capture.target->ui_state();
    if (state->kind == UIKind::Slider || state->kind == UIKind::ScrollBar)
      SetSlider(capture.target.get(), point, capture.slider_offset);
    else if (state->kind == UIKind::InputField) {
      state->caret =
          InputCursor(state, LocalPoint(capture.target.get(), point));
      capture.dragged = capture.dragged || state->caret != state->selection;
    } else if (capture.drag_target) {
      if (glm::length(point - capture.start) > 6) capture.dragged = true;
      if (capture.dragged) {
        state->pressed = false;
        auto scroll = capture.drag_target->ui_state();
        scroll->scroll_->data -=
            LocalPoint(capture.drag_target.get(), point) -
            LocalPoint(capture.drag_target.get(), capture.last);
        ClampScroll(scroll);
        Emit("scroll", capture.drag_target.get(), id, point);
      }
    }
    capture.last = point;
    Emit(capture.dragged ? "drag" : "pointer_move", capture.target.get(), id,
         point, LocalPoint(capture.target.get(), point));
    return true;
  }
  if ((type == "up" || type == "cancel") && captured != captures_.end()) {
    Capture capture = captured->second;
    captures_.erase(captured);
    auto state = capture.target->ui_state();
    if (state) {
      state->pressed = false;
      for (const auto& [other_id, other] : captures_)
        if (other.target == capture.target && !other.dragged)
          state->pressed = true;
    }
    const auto local = LocalPoint(capture.target.get(), point);
    Emit(type == "cancel" ? "pointer_cancel" : "pointer_up",
         capture.target.get(), id, point, local);
    if (type == "up" && !capture.dragged && node == capture.target && state &&
        state->accepts_input()) {
      Activate(capture.target.get(), local);
      Emit("click", capture.target.get(), id, point, local);
    }
    return true;
  }
  return node || Modal();
}
void UI::SetSlider(Node* node, glm::vec2 point, float offset) {
  auto state = node->ui_state();
  const auto p = LocalPoint(node, point);
  const auto rect = state->bounds();
  const float pad = std::clamp(static_cast<float>(state->style_->padding_),
                               0.0f, std::min(rect.z, rect.w) / 2);
  float fraction =
      state->vertical_
          ? (p.y - rect.y - pad) / std::max(1.0f, rect.w - 2 * pad)
          : (p.x - rect.x - pad) / std::max(1.0f, rect.z - 2 * pad);
  if (state->kind == UIKind::ScrollBar) {
    const auto thumb = state->ScrollThumb();
    const float track = (state->vertical_ ? rect.w : rect.z) - 2 * pad;
    const float coordinate = state->vertical_ ? p.y - rect.y : p.x - rect.x;
    fraction = track > thumb.y ? (coordinate - pad - thumb.y / 2 - offset) /
                                     (track - thumb.y)
                               : 0;
  }
  if (state->vertical_ && state->kind != UIKind::ScrollBar)
    fraction = 1 - fraction;
  float value = state->min_value_ +
                std::clamp(fraction, 0.0f, 1.0f) *
                    std::max(0.0f, state->max_value_ - state->min_value_);
  if (state->step_ > 0)
    value =
        state->min_value_ +
        std::round((value - state->min_value_) / state->step_) * state->step_;
  value = std::clamp(value, state->min_value_,
                     std::max(state->min_value_, state->max_value_));
  if (value != state->value_) {
    state->value_ = value;
    state->ApplyScrollValue();
    if (Disposable::Check(state->scroll_target_))
      Emit("scroll", state->scroll_target_.get());
    Emit("value_changed", node);
  }
}
void UI::Activate(Node* node, glm::vec2 local) {
  auto state = node->ui_state();
  if (state->kind == UIKind::Toggle) {
    state->checked_ = !state->checked_;
    Emit("value_changed", node);
  }
  if (state->kind == UIKind::List) {
    int index = static_cast<int>(
        (local.y - state->bounds().y + state->scroll_->data.y) /
        std::max(1.0f, state->row_height_));
    if (index >= 0 && index < static_cast<int>(state->items_.size()) &&
        index != state->selected_index_) {
      state->selected_index_ = index;
      Emit("selection_changed", node);
    }
  }
  if (state->kind == UIKind::Dropdown) {
    const float y = local.y - state->bounds().y;
    if (state->expanded_ && y >= state->bounds().w) {
      const int index =
          static_cast<int>((y - state->bounds().w + state->scroll_->data.y) /
                           std::max(1.0f, state->row_height_));
      if (index >= 0 && index < static_cast<int>(state->items_.size())) {
        state->selected_index_ = index;
        Emit("selection_changed", node);
      }
      state->expanded_ = false;
    } else
      state->expanded_ = !state->expanded_;
  }
}
void UI::MoveFocus(int direction) {
  std::vector<Node*> candidates;
  Node* modal = Modal();
  std::function<void(Node*)> visit = [&](Node* node) {
    if (!node->visible_in_tree()) return;
    if (auto state = node->ui_state(); state && state->focusable_ &&
                                       state->accepts_input() &&
                                       (!modal || Descendant(node, modal)))
      candidates.push_back(node);
    for (auto child : node->children_raw()) visit(child);
  };
  if (Graphics::TryGet()) visit(Graphics::Get().root().get());
  if (candidates.empty()) return;
  auto it = std::find(candidates.begin(), candidates.end(), focused_.get());
  int index = it == candidates.end()
                  ? (direction < 0 ? 0 : -1)
                  : static_cast<int>(it - candidates.begin());
  index = (index + direction + static_cast<int>(candidates.size())) %
          static_cast<int>(candidates.size());
  Focus(candidates[index]);
}
bool UI::ReplaceSelection(std::string insertion) {
  if (!focused_ || focused_->ui_state()->kind != UIKind::InputField)
    return false;
  auto state = focused_->ui_state();
  if (state->read_only_ || !state->accepts_input()) return true;
  insertion = ui_text::Valid(insertion);
  insertion.erase(std::remove(insertion.begin(), insertion.end(), '\r'),
                  insertion.end());
  if (!state->multiline_)
    insertion.erase(std::remove(insertion.begin(), insertion.end(), '\n'),
                    insertion.end());
  const size_t a =
      ui_text::Boundary(state->text_, std::min(state->caret, state->selection));
  const size_t b =
      ui_text::Boundary(state->text_, std::max(state->caret, state->selection));
  if (state->character_limit_ > 0) {
    const size_t retained =
        ui_text::Count(state->text_.substr(0, a) + state->text_.substr(b));
    insertion = ui_text::Limit(
        insertion, std::max<int64_t>(0, state->character_limit_ -
                                            static_cast<int64_t>(retained)));
  }
  const std::string result =
      state->text_.substr(0, a) + insertion + state->text_.substr(b);
  if (state->numeric_) {
    int dots = 0;
    for (size_t i = 0; i < result.size(); ++i)
      if (result[i] == '.') {
        if (++dots > 1) return true;
      } else if (result[i] == '-') {
        if (i != 0) return true;
      } else if (result[i] < '0' || result[i] > '9')
        return true;
  }
  if (result != state->text_) {
    state->text_ = result;
    state->caret = state->selection = a + insertion.size();
    Emit("value_changed", focused_.get());
  }
  state->composition.clear();
  return true;
}
bool UI::Text(std::string text) { return ReplaceSelection(std::move(text)); }
bool UI::Key(int32_t key, bool shift, bool control) {
  Update();
  // Numeric keypad navigation is useful on lists and other non-text widgets.
  // In text fields keep digit entry while Num Lock is enabled.
  if (!focused_ || focused_->ui_state()->kind != UIKind::InputField ||
      !(SDL_GetModState() & SDL_KMOD_NUM)) {
    switch (key) {
      case SDL_SCANCODE_KP_2:
        key = SDL_SCANCODE_DOWN;
        break;
      case SDL_SCANCODE_KP_4:
        key = SDL_SCANCODE_LEFT;
        break;
      case SDL_SCANCODE_KP_6:
        key = SDL_SCANCODE_RIGHT;
        break;
      case SDL_SCANCODE_KP_8:
        key = SDL_SCANCODE_UP;
        break;
      case SDL_SCANCODE_KP_1:
        key = SDL_SCANCODE_END;
        break;
      case SDL_SCANCODE_KP_7:
        key = SDL_SCANCODE_HOME;
        break;
      case SDL_SCANCODE_KP_PERIOD:
        key = SDL_SCANCODE_DELETE;
        break;
      default:
        break;
    }
  }
  if (key == SDL_SCANCODE_TAB) {
    MoveFocus(shift ? -1 : 1);
    return focused_ || Modal();
  }
  auto state = focused_ ? focused_->ui_state() : nullptr;
  if (state && state->kind == UIKind::InputField) {
    state->caret = ui_text::Boundary(state->text_, state->caret);
    state->selection = ui_text::Boundary(state->text_, state->selection);
    if (!state->composition.empty() &&
        (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT ||
         key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_BACKSPACE))
      return true;
    if (control && key == SDL_SCANCODE_A) {
      state->selection = 0;
      state->caret = state->text_.size();
      return true;
    }
    if (control && (key == SDL_SCANCODE_C || key == SDL_SCANCODE_X)) {
      if (!state->password_ && state->caret != state->selection) {
        const auto a = std::min(state->caret, state->selection),
                   b = std::max(state->caret, state->selection);
        SDL_SetClipboardText(state->text_.substr(a, b - a).c_str());
        if (key == SDL_SCANCODE_X) ReplaceSelection("");
      }
      return true;
    }
    if (control && key == SDL_SCANCODE_V) {
      char* text = SDL_GetClipboardText();
      if (text) {
        ReplaceSelection(text);
        SDL_free(text);
      }
      return true;
    }
    if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT) {
      if (!shift && state->caret != state->selection)
        state->caret = key == SDL_SCANCODE_LEFT
                           ? std::min(state->caret, state->selection)
                           : std::max(state->caret, state->selection);
      else
        state->caret = key == SDL_SCANCODE_LEFT
                           ? ui_text::Previous(state->text_, state->caret)
                           : ui_text::Next(state->text_, state->caret);
      if (!shift) state->selection = state->caret;
      return true;
    }
    if (key == SDL_SCANCODE_HOME || key == SDL_SCANCODE_END) {
      if (state->multiline_ && !control) {
        if (key == SDL_SCANCODE_HOME) {
          auto start = state->caret ? state->text_.rfind('\n', state->caret - 1)
                                    : std::string::npos;
          state->caret = start == std::string::npos ? 0 : start + 1;
        } else {
          auto end = state->text_.find('\n', state->caret);
          state->caret = end == std::string::npos ? state->text_.size() : end;
        }
      } else
        state->caret = key == SDL_SCANCODE_HOME ? 0 : state->text_.size();
      if (!shift) state->selection = state->caret;
      return true;
    }
    if (state->multiline_ &&
        (key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN)) {
      auto start = state->caret ? state->text_.rfind('\n', state->caret - 1)
                                : std::string::npos;
      start = start == std::string::npos ? 0 : start + 1;
      auto end = state->text_.find('\n', state->caret);
      end = end == std::string::npos ? state->text_.size() : end;
      const float column =
          Measure(state, state->text_.substr(start, state->caret - start));
      size_t next_start = start, next_end = end;
      if (key == SDL_SCANCODE_UP && start > 0) {
        next_end = start - 1;
        auto previous = next_end ? state->text_.rfind('\n', next_end - 1)
                                 : std::string::npos;
        next_start = previous == std::string::npos ? 0 : previous + 1;
      } else if (key == SDL_SCANCODE_DOWN && end < state->text_.size()) {
        next_start = end + 1;
        next_end = state->text_.find('\n', next_start);
        if (next_end == std::string::npos) next_end = state->text_.size();
      }
      size_t cursor = next_start;
      while (cursor < next_end) {
        auto next = ui_text::Next(state->text_, cursor);
        float a = Measure(state,
                          state->text_.substr(next_start, cursor - next_start));
        float b =
            Measure(state, state->text_.substr(next_start, next - next_start));
        if (column < (a + b) / 2) break;
        cursor = next;
      }
      state->caret = cursor;
      if (!shift) state->selection = cursor;
      return true;
    }
    if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_DELETE) {
      if (state->read_only_) return true;
      if (state->caret == state->selection)
        state->selection = key == SDL_SCANCODE_BACKSPACE
                               ? ui_text::Previous(state->text_, state->caret)
                               : ui_text::Next(state->text_, state->caret);
      ReplaceSelection("");
      return true;
    }
    if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER) {
      if (state->multiline_ && !control)
        ReplaceSelection("\n");
      else
        Emit("submit", focused_.get());
      return true;
    }
    if (key != SDL_SCANCODE_ESCAPE) return true;
  }
  if (key == SDL_SCANCODE_ESCAPE) {
    if (state && state->kind == UIKind::Dropdown && state->expanded_) {
      state->expanded_ = false;
      return true;
    }
    if (Node* modal = Modal()) {
      Emit("cancel", modal);
      return true;
    }
    if (focused_) {
      Emit("cancel", focused_.get());
      Focus(nullptr);
      return true;
    }
    return false;
  }
  if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_SPACE ||
      key == SDL_SCANCODE_KP_ENTER) {
    if (focused_ && state->accepts_input()) {
      if (state->kind == UIKind::Dropdown)
        state->expanded_ = !state->expanded_;
      else if (state->kind == UIKind::List) {
        if (state->selected_index_ < 0 && !state->items_.empty()) {
          state->selected_index_ = 0;
          Emit("selection_changed", focused_.get());
        }
        Emit("submit", focused_.get());
      } else
        Activate(focused_.get(), glm::vec2(state->bounds()));
      Emit("click", focused_.get());
      return true;
    }
    if (Modal()) {
      MoveFocus(1);
      return true;
    }
    return false;
  }
  if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_UP ||
      key == SDL_SCANCODE_RIGHT || key == SDL_SCANCODE_DOWN) {
    const int direction =
        (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_UP) ? -1 : 1;
    if (state &&
        (state->kind == UIKind::Slider || state->kind == UIKind::ScrollBar)) {
      float step =
          state->step_ > 0
              ? state->step_
              : std::max(0.0f, state->max_value_ - state->min_value_) * 0.05f;
      state->value_ =
          std::clamp(state->value_ + direction * step, state->min_value_,
                     std::max(state->min_value_, state->max_value_));
      state->ApplyScrollValue();
      if (Disposable::Check(state->scroll_target_))
        Emit("scroll", state->scroll_target_.get());
      Emit("value_changed", focused_.get());
      return true;
    }
    if (state &&
        (state->kind == UIKind::List || state->kind == UIKind::Dropdown) &&
        !state->items_.empty()) {
      state->selected_index_ =
          std::clamp(state->selected_index_ + direction, 0,
                     static_cast<int>(state->items_.size()) - 1);
      float row = std::max(1.0f, state->row_height_),
            y = state->selected_index_ * row;
      if (y < state->scroll_->data.y) state->scroll_->data.y = y;
      const float visible =
          state->kind == UIKind::Dropdown
              ? std::min<size_t>(8, state->items_.size()) * row
              : state->bounds().w;
      if (y + row > state->scroll_->data.y + visible)
        state->scroll_->data.y = y + row - visible;
      ClampScroll(state);
      Emit("selection_changed", focused_.get());
      return true;
    }
    if (focused_ || Modal()) {
      MoveFocus(direction);
      return true;
    }
  }
  return Modal() != nullptr;
}
bool UI::Scroll(float x, float y, float dx, float dy) {
  Update();
  const auto hit = Hit(glm::vec2(x, y));
  for (Node* node = hit ? hit->target.get() : nullptr; node;
       node = node->scene_parent())
    if (auto state = node->ui_state();
        state && state->accepts_input() &&
        (state->kind == UIKind::ScrollPanel || state->kind == UIKind::List ||
         state->kind == UIKind::Dropdown)) {
      state->scroll_->data -= glm::vec2(dx, dy) * state->scroll_speed_;
      ClampScroll(state);
      Update();
      Emit("scroll", node);
      return true;
    }
  return Modal() != nullptr;
}
bool UI::KeyboardBlocked() const {
  return Modal() || (focused_ && focused_->ui_state() &&
                     focused_->ui_state()->kind == UIKind::InputField);
}
bool UI::PointerBlocked() const {
  return Modal() || hovered_ || !captures_.empty();
}
bool UI::ProcessEvent(const SDL_Event& event) {
  if (!Graphics::TryGet() || canvases_.empty()) return false;
  auto& graphics = Graphics::Get();
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      if (event.motion.which == SDL_TOUCH_MOUSEID) {
        auto p = graphics.WindowToScreen(event.motion.x, event.motion.y);
        return Hit(p) || Modal();
      }
      {
        auto p = graphics.WindowToScreen(event.motion.x, event.motion.y);
        return Pointer("move", p.x, p.y);
      }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event.button.button != SDL_BUTTON_LEFT) return false;
      if (event.button.which == SDL_TOUCH_MOUSEID) {
        auto p = graphics.WindowToScreen(event.button.x, event.button.y);
        return Hit(p) || Modal();
      }
      {
        auto p = graphics.WindowToScreen(event.button.x, event.button.y);
        return Pointer(
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "down" : "up", p.x,
            p.y);
      }
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED: {
      auto p = graphics.NormalizedToScreen(event.tfinger.x, event.tfinger.y);
      auto key = std::make_pair(event.tfinger.touchID, event.tfinger.fingerID);
      auto found = touch_ids_.find(key);
      if (found == touch_ids_.end())
        found = touch_ids_.emplace(key, next_touch_id_++).first;
      const auto id = found->second;
      bool handled =
          Pointer(event.type == SDL_EVENT_FINGER_DOWN       ? "down"
                  : event.type == SDL_EVENT_FINGER_UP       ? "up"
                  : event.type == SDL_EVENT_FINGER_CANCELED ? "cancel"
                                                            : "move",
                  p.x, p.y, id);
      if (event.type == SDL_EVENT_FINGER_UP ||
          event.type == SDL_EVENT_FINGER_CANCELED)
        touch_ids_.erase(key);
      return handled;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      // Windows raw mouse motion can be pumped after the wheel event was
      // created, leaving its stored coordinates at the previous control.
      float x = 0, y = 0;
      SDL_GetMouseState(&x, &y);
      auto p = graphics.WindowToScreen(x, y);
      float sign =
          event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
      return Scroll(p.x, p.y, event.wheel.x * sign, event.wheel.y * sign);
    }
    case SDL_EVENT_KEY_DOWN:
      return Key(event.key.scancode, (event.key.mod & SDL_KMOD_SHIFT) != 0,
                 (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0);
    case SDL_EVENT_TEXT_INPUT:
      return Text(event.text.text ? event.text.text : "");
    case SDL_EVENT_TEXT_EDITING:
      if (focused_ && focused_->ui_state()->kind == UIKind::InputField) {
        focused_->ui_state()->composition =
            event.edit.text ? event.edit.text : "";
        return true;
      }
      return false;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
          return Key(SDL_SCANCODE_UP);
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
          return Key(SDL_SCANCODE_DOWN);
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
          return Key(SDL_SCANCODE_LEFT);
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
          return Key(SDL_SCANCODE_RIGHT);
        case SDL_GAMEPAD_BUTTON_SOUTH:
          return Key(SDL_SCANCODE_RETURN);
        case SDL_GAMEPAD_BUTTON_EAST:
          return Key(SDL_SCANCODE_ESCAPE);
        default:
          return KeyboardBlocked();
      }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      CancelPointers();
      Focus(nullptr);
      return false;
    default:
      return false;
  }
}
}  // namespace urge
