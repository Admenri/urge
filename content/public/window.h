// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_WINDOW_H_
#define CONTENT_PUBLIC_WINDOW_H_

#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/utility.h"
#include "content/public/viewport.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"

namespace content {

class WindowControlLayer;

class Window : public base::RefCounted<Window>,
               public GraphicsElement,
               public Disposable,
               public ViewportChild {
 public:
  Window(scoped_refptr<Graphics> screen,
         scoped_refptr<Viewport> viewport = nullptr);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  void Update();

  void SetWindowskin(scoped_refptr<Bitmap> windowskin);
  scoped_refptr<Bitmap> GetWindowskin() const {
    CheckIsDisposed();
    return windowskin_;
  }

  void SetContents(scoped_refptr<Bitmap> contents);
  scoped_refptr<Bitmap> GetContents() const {
    CheckIsDisposed();
    return contents_;
  }

  void SetStretch(bool stretch);
  bool GetStretch() const {
    CheckIsDisposed();
    return stretch_;
  }

  void SetCursorRect(scoped_refptr<Rect> cursor_rect);
  scoped_refptr<Rect> GetCursorRect() const {
    CheckIsDisposed();
    return cursor_rect_;
  }

  void SetActive(bool active);
  bool GetActive() const {
    CheckIsDisposed();
    return active_;
  }

  void SetPause(bool pause);
  bool GetPause() const {
    CheckIsDisposed();
    return pause_;
  }

  void SetX(int v);
  int GetX() const {
    CheckIsDisposed();
    return rect_.x;
  }

  void SetY(int v);
  int GetY() const {
    CheckIsDisposed();
    return rect_.y;
  }

  void SetWidth(int v);
  int GetWidth() const {
    CheckIsDisposed();
    return rect_.width;
  }

  void SetHeight(int v);
  int GetHeight() const {
    CheckIsDisposed();
    return rect_.height;
  }

  void SetOX(int v);
  int GetOX() const {
    CheckIsDisposed();
    return origin_.x;
  }

  void SetOY(int v);
  int GetOY() const {
    CheckIsDisposed();
    return origin_.y;
  }

  void SetOpacity(int v);
  int GetOpacity() const {
    CheckIsDisposed();
    return opacity_;
  }

  void SetBackOpacity(int v);
  int GetBackOpacity() const {
    CheckIsDisposed();
    return back_opacity_;
  }

  void SetContentsOpacity(int v);
  int GetContentsOpacity() const {
    CheckIsDisposed();
    return contents_opacity_;
  }

  void SetZ(int v) override;
  void SetVisible(bool visible) override;
  void SetViewport(scoped_refptr<Viewport> viewport) override;

 private:
  friend class WindowControlLayer;
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Window"; }
  void PrepareDraw() override;
  void OnDraw(CompositeTargetInfo* target_info) override;
  void CheckObjectDisposed() const override { CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& rect) override;

  void InitWindow();
  void ResetBaseTexQuadsInternal();
  void UpdateBaseTexInternal();
  void UpdateControlsQuadsInternal();
  void UpdateControlsInternal();
  void CursorRectChangedInternal();
  void CompositeControls(CompositeTargetInfo* target_info);

  scoped_refptr<Bitmap> windowskin_;
  scoped_refptr<Bitmap> contents_;
  bool stretch_ = true;
  scoped_refptr<Rect> cursor_rect_;
  bool active_ = true;
  bool pause_ = false;
  base::Rect rect_;
  base::Vec2i origin_;
  int opacity_ = 255;
  int back_opacity_ = 255;
  int contents_opacity_ = 255;

  std::unique_ptr<WindowControlLayer> control_layer_;

  struct WindowRendererData {
    std::unique_ptr<renderer::QuadDrawable> base_quad;
    std::unique_ptr<renderer::QuadDrawable> content_quad;
    std::unique_ptr<renderer::QuadArray> controls_quads;
    std::unique_ptr<renderer::QuadArray> base_tex_quad_array;
  };

  std::unique_ptr<WindowRendererData> renderer_data_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> base_texture_;

  int background_quads_count_ = 0;
  int controls_quad_count = 0;
  renderer::GeometryVertexLayout::Data* cursor_vertex_ = nullptr;
  renderer::GeometryVertexLayout::Data* pause_vertex_ = nullptr;
  int pause_quad_index_ = 0;
  int pause_alpha_index_ = 0;
  int cursor_alpha_index_ = 0;

  bool update_required_ = false;
  bool base_tex_need_update_ = false;
  bool base_quad_need_update_ = false;
  bool controls_quads_need_update_ = true;
  bool contents_quad_need_update_ = true;

  base::CallbackListSubscription cursor_rect_observer_;
  base::CallbackListSubscription windowskin_observer_;

  base::WeakPtrFactory<Window> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_WINDOW_H_
