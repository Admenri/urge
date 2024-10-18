// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_DRAWABLE_H_
#define CONTENT_PUBLIC_DRAWABLE_H_

#include "base/containers/linked_list.h"
#include "base/math/rectangle.h"

#include "renderer/device/render_device.h"

namespace content {

class Drawable;

class DrawableParent {
 public:
  struct ViewportInfo {
    base::Rect rect;
    base::Vec2i origin;

    // Did viewport container has scissor area?
    bool has_scissor = true;

    // Compute viewport offset
    base::Vec2i GetRealOffset() const { return rect.Position() - origin; }
  };

  DrawableParent() = default;
  virtual ~DrawableParent();

  DrawableParent(const DrawableParent&) = delete;
  DrawableParent& operator=(const DrawableParent&) = delete;

  void InsertDrawable(Drawable* drawable);
  void AddChild(Drawable* drawable);

  // Composite screen
  void PrepareComposite();
  void Composite();

  // Viewport rect
  void NotifyViewportRectChanged();
  inline ViewportInfo& viewport_rect() { return viewport_rect_; }

 private:
  bool GetDrawableOrder(Drawable* self, Drawable* other);

  ViewportInfo viewport_rect_;
  base::LinkedList<Drawable> drawables_;
  base::LinkedList<Drawable> children_;
};

class Drawable {
 public:
  Drawable(DrawableParent* parent,
           int z = 0,
           bool visible = true,
           int sprite_y = 0);
  virtual ~Drawable();

  Drawable(const Drawable&) = delete;
  Drawable& operator=(const Drawable&) = delete;

  void SetParent(DrawableParent* parent);
  DrawableParent* GetParent() const {
    CheckObjectDisposed();
    return parent_;
  }

  virtual void SetZ(int z);
  int GetZ() const {
    CheckObjectDisposed();
    return z_;
  }

  virtual void SetVisible(bool visible) {
    CheckObjectDisposed();
    visible_ = visible;
  }

  bool GetVisible() const {
    CheckObjectDisposed();
    return visible_;
  }

  inline DrawableParent::ViewportInfo& parent_rect() {
    return parent_->viewport_rect();
  }

 protected:
  virtual void PrepareDraw() {}
  virtual void OnDraw() = 0;
  virtual void CheckObjectDisposed() const = 0;
  virtual void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo&) {}

  void RemoveFromList();
  void SetSpriteY(int y);

 private:
  friend class DrawableParent;

  base::LinkNode<Drawable> draw_node_;
  base::LinkNode<Drawable> child_node_;

  DrawableParent* parent_;
  int z_;
  bool visible_;
  uint64_t creation_stamp_;
  int sprite_y_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_DRAWABLE_H_