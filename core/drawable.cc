// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/drawable.h"

namespace urge {

Drawable::Drawable() : prev_(this), next_(this) {}

Drawable::Drawable(OnPrepare on_prepare, OnDraw on_draw, const ZValue& order)
    : on_prepare_(on_prepare),
      on_draw_(on_draw),
      prev_(this),
      next_(this),
      z_(order) {}

Drawable::~Drawable() {
  RemoveFromList();
}

void Drawable::SortWith(ZValue value) {
  if (value != z_) {
    auto old_z = z_;
    z_ = value;
    Resort(old_z);
  }
}

void Drawable::Resort(ZValue old) {
  if (parent_ != nullptr) {
    if (z_ < old) {
      BubbleLeft();
    } else {
      BubbleRight();
    }
  }
}

void Drawable::SetParent(DrawableSet* parent) {
  if (parent_ != parent) {
    RemoveFromList();
    parent_ = parent;
    if (parent_) {
      // Insert into new list at correct z-order position:
      // find the first node whose z > this->z_, then insert before it.
      Drawable* sentinel = &parent_->root_;
      Drawable* pos = sentinel->next_;
      while (pos != sentinel && pos->z_ <= z_)
        pos = pos->next_;
      InsertAfter(pos->prev_);  // insert before 'pos'
    }
  }
}

void Drawable::RemoveFromList() {
  prev_->next_ = next_;
  next_->prev_ = prev_;
  prev_ = this;
  next_ = this;
}

void Drawable::InsertAfter(Drawable* node) {
  next_ = node->next_;
  prev_ = node;
  node->next_->prev_ = this;
  node->next_ = this;
}

void Drawable::BubbleLeft() {
  // z decreased: move left past any node with higher z
  while (prev_ != &parent_->root_ && prev_->z_ > z_) {
    Drawable* target = prev_;
    RemoveFromList();
    // re-insert before target
    prev_ = target->prev_;
    next_ = target;
    target->prev_->next_ = this;
    target->prev_ = this;
  }
}

void Drawable::BubbleRight() {
  // z increased: move right past any node with lower z
  while (next_ != &parent_->root_ && next_->z_ < z_) {
    Drawable* target = next_;
    RemoveFromList();
    // re-insert after target
    next_ = target->next_;
    prev_ = target;
    target->next_->prev_ = this;
    target->next_ = this;
  }
}

// -----------------------------------------------------------------

DrawableSet::DrawableSet() {
  root_.prev_ = &root_;
  root_.next_ = &root_;
}

DrawableSet::~DrawableSet() {
  // Detach all remaining drawables so they don't hold dangling parent
  // pointers.
  while (root_.next_ != &root_) {
    root_.next_->parent_ = nullptr;
    root_.next_->RemoveFromList();
  }
}

void DrawableSet::DispatchPrepare(DrawParam param) {
  for (Drawable* node = root_.next_; node != &root_; node = node->next_)
    if (node->visible_)
      node->on_prepare_(param);
}

void DrawableSet::DispatchDraw(DrawParam param) {
  for (Drawable* node = root_.next_; node != &root_; node = node->next_)
    if (node->visible_)
      node->on_draw_(param);
}

}  // namespace urge
