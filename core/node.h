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

#pragma once

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/disposable.h"
#include "core/drawable.h"
#include "core/utility.h"

namespace urge {

class Node : public Disposable {
 public:
  explicit Node();
  Node(RefPtr<Node> parent, const ZValue& z);

  /*-export.begin-*/
  Node(RefPtr<Node> parent);
  ~Node() override;

  void Render(RefPtr<Bitmap> target, RefPtr<Color> clear = nullptr);

  virtual ATTR(int32_t, Z);
  virtual ATTR(bool, Visible);
  virtual ATTR(RefPtr<Node>, Parent);

  virtual ATTR(RefPtr<Vector3>, Position);
  virtual ATTR(RefPtr<Vector4>, Quaternion);
  virtual ATTR(RefPtr<Vector3>, Scale);
  /*-export.end-*/

 protected:
  virtual bool Prepare(DrawParam param) { return false; }
  virtual bool DoDraw(DrawParam param) { return false; }
  virtual void PostDraw(DrawParam param) {}

  void DisposeObject() override;

 public:
  ZValue GetOrder() { return self_.order(); }
  void SortWith(ZValue v) { return self_.SortWith(v); }

  Drawable* PrevDrawable() { return self_.Prev(); }
  Drawable* NextDrawable() { return self_.Next(); }

  template <class Ty>
  void SetupTrait(Ty* self) {
    self_.SetupTrait<Ty>(self);
  }
  template <class Ty>
  Ty* TryCast() {
    return self_.TryCast<Ty>();
  }

  glm::mat4 local_transform() { return transform_.local; }
  glm::mat4 world_transform() { return transform_.world; }

 private:
  void ExecutePrepare(DrawParam param);
  void ExecuteRendering(DrawParam param);
  void RebuildModelTransform();

  DrawableSet children_;
  Drawable self_;
  RefPtr<Node> parent_;
  bool world_root_ = false;

  bool allow_do_draw_ = false;
  bool allow_post_draw_ = false;

  struct Transform3D {
    RefPtr<Vector3> position;
    RefPtr<Vector4> quaternion;
    RefPtr<Vector3> scale;
    glm::mat4 local = glm::mat4(1.0f);
    glm::mat4 world = glm::mat4(1.0f);

    Transform3D()
        : position(MakeRefCounted<Vector3>(0.0f)),
          quaternion(MakeRefCounted<Vector4>(0.0f, 0.0f, 0.0f, 1.0f)),
          scale(MakeRefCounted<Vector3>(1.0f)) {}
  } transform_;
};

}  // namespace urge
