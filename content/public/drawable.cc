// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/drawable.h"

namespace content {

static uint64_t g_creation_stamp = 0;

DrawableParent::~DrawableParent() {
  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next())
    it->value()->parent_ = nullptr;
}

void DrawableParent::InsertDrawable(Drawable* drawable) {
  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next())
    if (GetDrawableOrder(it->value(), drawable))
      return drawables_.InsertBefore(it, &drawable->draw_node_);

  drawables_.Append(&drawable->draw_node_);
}

void DrawableParent::AddChild(Drawable* drawable) {
  children_.Append(&drawable->child_node_);
}

void DrawableParent::PrepareComposite() {
  if (children_.empty())
    return;

  for (auto it = children_.tail(); it != children_.end(); it = it->previous()) {
    auto* child = it->value();
    if (child->visible_)
      child->PrepareDraw();
  }
}

void DrawableParent::Composite(CompositeTargetInfo* target_info) {
  if (drawables_.empty())
    return;

  for (auto it = drawables_.tail(); it != drawables_.end(); it = it->previous())
    if (it->value()->visible_)
      it->value()->OnDraw(target_info);
}

void DrawableParent::NotifyViewportRectChanged() {
  for (auto* it = drawables_.head(); it != drawables_.end(); it = it->next())
    it->value()->OnParentViewportRectChanged(viewport_rect_);
}

bool DrawableParent::GetDrawableOrder(Drawable* self, Drawable* other) {
  if (self->z_ < other->z_) {
    return true;
  }

  if (self->z_ == other->z_) {
    if (self->sprite_y_ == other->sprite_y_)
      return (self->creation_stamp_ < other->creation_stamp_);
    else
      return (self->sprite_y_ < other->sprite_y_);
  }

  return false;
}

Drawable::Drawable(DrawableParent* parent, int z, bool visible, int sprite_y)
    : draw_node_(this),
      child_node_(this),
      parent_(parent),
      z_(z),
      visible_(visible),
      creation_stamp_(++g_creation_stamp),
      sprite_y_(sprite_y) {
  parent_->InsertDrawable(this);
  parent_->AddChild(this);
}

Drawable::~Drawable() {
  RemoveFromList();
}

void Drawable::SetParent(DrawableParent* parent) {
  CheckObjectDisposed();

  if (parent_ == parent)
    return;
  parent_ = parent;

  RemoveFromList();
  parent_->InsertDrawable(this);
  parent_->AddChild(this);

  OnParentViewportRectChanged(parent->viewport_rect());
}

void Drawable::SetZ(int z) {
  CheckObjectDisposed();

  if (z_ == z)
    return;
  z_ = z;

  draw_node_.RemoveFromList();
  parent_->InsertDrawable(this);
}

void Drawable::RemoveFromList() {
  draw_node_.RemoveFromList();
  child_node_.RemoveFromList();
}

void Drawable::SetSpriteY(int y) {
  sprite_y_ = y;

  draw_node_.RemoveFromList();
  parent_->InsertDrawable(this);
}

}  // namespace content