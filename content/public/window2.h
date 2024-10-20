// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_WINDOW2_H_
#define CONTENT_PUBLIC_WINDOW2_H_

#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/graphics.h"
#include "content/public/utility.h"
#include "content/public/viewport.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"

namespace content {

/* Adaptor for RGSS 2/3 */
class Window2 : public base::RefCounted<Window2>,
                public GraphicsElement,
                public Disposable,
                public ViewportChild {
 public:
  Window2(scoped_refptr<Graphics> screen,
          scoped_refptr<Viewport> viewport = nullptr);
  Window2(scoped_refptr<Graphics> screen,
          int x = 0,
          int y = 0,
          int width = 0,
          int height = 0);
  ~Window2() override;

  Window2(const Window2&) = delete;
  Window2& operator=(const Window2&) = delete;

  /* Method */
  void Update();
  void Move(int x, int y, int width, int height);

  bool IsOpened() const {
    CheckIsDisposed();
    return openness_ == 255;
  }

  bool IsClosed() const {
    CheckIsDisposed();
    return openness_ == 0;
  }

  /* Attribute */
  scoped_refptr<Bitmap> GetWindowskin() const {
    CheckIsDisposed();
    return windowskin_;
  }

  void SetWindowskin(scoped_refptr<Bitmap> windowskin);

  scoped_refptr<Bitmap> GetContents() const {
    CheckIsDisposed();
    return contents_;
  }

  void SetContents(scoped_refptr<Bitmap> contents);

  scoped_refptr<Rect> GetCursorRect() const {
    CheckIsDisposed();
    return cursor_rect_;
  }

  void SetCursorRect(scoped_refptr<Rect> cursor_rect);

  bool GetActive() const {
    CheckIsDisposed();
    return active_;
  }

  void SetActive(bool active);

  bool GetArrowsVisible() const {
    CheckIsDisposed();
    return arrows_visible_;
  }

  void SetArrowsVisible(bool arrows_visible);

  bool GetPause() const {
    CheckIsDisposed();
    return pause_;
  }

  void SetPause(bool pause);

  int GetX() const {
    CheckIsDisposed();
    return rect_.x;
  }

  void SetX(int x);

  int GetY() const {
    CheckIsDisposed();
    return rect_.y;
  }

  void SetY(int y);

  int GetWidth() const {
    CheckIsDisposed();
    return rect_.width;
  }

  void SetWidth(int width);

  int GetHeight() const {
    CheckIsDisposed();
    return rect_.height;
  }

  void SetHeight(int height);

  int GetOX() const {
    CheckIsDisposed();
    return ox_;
  }

  void SetOX(int ox);

  int GetOY() const {
    CheckIsDisposed();
    return oy_;
  }

  void SetOY(int oy);

  int GetPadding() const {
    CheckIsDisposed();
    return padding_;
  }

  void SetPadding(int padding);

  int GetPaddingBottom() const {
    CheckIsDisposed();
    return padding_bottom_;
  }

  void SetPaddingBottom(int padding_bottom);

  int GetOpacity() const {
    CheckIsDisposed();
    return opacity_;
  }

  void SetOpacity(int opacity);

  int GetBackOpacity() const {
    CheckIsDisposed();
    return back_opacity_;
  }

  void SetBackOpacity(int back_opacity);

  int GetContentsOpacity() const {
    CheckIsDisposed();
    return contents_opacity_;
  }

  void SetContentsOpacity(int contents_opacity);

  int GetOpenness() const {
    CheckIsDisposed();
    return openness_;
  }

  void SetOpenness(int openness);

  scoped_refptr<Tone> GetTone() const {
    CheckIsDisposed();
    return tone_;
  }

  void SetTone(scoped_refptr<Tone> tone);

 private:
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Window2"; }

  void PrepareDraw() override;
  void OnDraw(CompositeTargetInfo* target_info) override;
  void CheckObjectDisposed() const override;
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& rect) override;

  void InitWindow();
  void UpdateInternal();

  void CalcBaseQuadArrayInternal();
  void UpdateBaseTextureInternal();
  void UpdateBaseQuadInternal();
  void CalcArrowsQuadArrayInternal();
  void CursorRectChangedInternal();
  void WindowskinChangedInternal();
  void UpdatePauseStepInternal();
  void UpdateCursorQuadsInternal();
  void ToneChangedInternal();

  scoped_refptr<Bitmap> windowskin_;
  base::CallbackListSubscription windowskin_observer_;
  scoped_refptr<Bitmap> contents_;
  scoped_refptr<Rect> cursor_rect_;
  base::CallbackListSubscription cursor_rect_observer_;
  bool active_ = true;
  bool arrows_visible_ = true;
  bool pause_ = false;

  base::Rect rect_;
  int ox_ = 0, oy_ = 0;

  int padding_ = 12;
  int padding_bottom_ = padding_;

  int opacity_ = 255;
  int back_opacity_ = 255;
  int contents_opacity_ = 255;
  int openness_ = 255;

  scoped_refptr<Tone> tone_;
  base::CallbackListSubscription tone_observer_;

  base::Rect padding_rect_;

  struct Window2RendererData {
    std::unique_ptr<renderer::QuadDrawable> base_quad;
    std::unique_ptr<renderer::QuadDrawable> content_quad;
    std::unique_ptr<renderer::QuadArray> arrows_quads;
    std::unique_ptr<renderer::QuadArray> cursor_quads;
    std::unique_ptr<renderer::QuadArray> base_tex_quad_array;
  };

  std::unique_ptr<Window2RendererData> renderer_data_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> base_texture_;

  bool update_required_ = false;

  bool base_tex_need_update_ = true;
  bool base_quad_need_update_ = true;
  bool contents_quad_need_update_ = false;
  bool cursor_step_need_update_ = false;
  bool arrows_quad_need_update_ = true;
  bool cursor_quad_need_update_ = true;

  size_t base_bg_tile_count_ = 0;
  size_t base_frame_tile_count_ = 0;
  size_t arrows_quad_count_ = 0;

  int pause_quad_index_ = 0;
  int pause_alpha_index_ = 0;
  int cursor_alpha_index_ = 0;

  renderer::GeometryVertexLayout::Data* pause_vertex_ = nullptr;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_WINDOW2_H_