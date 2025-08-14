// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_SURFACE_IMPL_H_
#define CONTENT_CANVAS_SURFACE_IMPL_H_

#include "SDL3/SDL_surface.h"

#include "content/canvas/font_impl.h"
#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_surface.h"

namespace content {

class SurfaceImpl : public Surface, public EngineObject, public Disposable {
 public:
  SurfaceImpl(ExecutionContext* context, SDL_Surface* surface);
  ~SurfaceImpl() override;

  SurfaceImpl(const SurfaceImpl&) = delete;
  SurfaceImpl& operator=(const SurfaceImpl&) = delete;

  static scoped_refptr<SurfaceImpl> From(scoped_refptr<Surface> host);

  SDL_Surface* GetRawSurface() const { return surface_; }

 public:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint32_t Width(ExceptionState& exception_state) override;
  uint32_t Height(ExceptionState& exception_state) override;
  scoped_refptr<Rect> GetRect(ExceptionState& exception_state) override;
  void Blt(int32_t x,
           int32_t y,
           scoped_refptr<Surface> src_surface,
           scoped_refptr<Rect> src_rect,
           uint32_t opacity,
           ExceptionState& exception_state) override;
  void StretchBlt(scoped_refptr<Rect> dest_rect,
                  scoped_refptr<Surface> src_surface,
                  scoped_refptr<Rect> src_rect,
                  uint32_t opacity,
                  ExceptionState& exception_state) override;
  void FillRect(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void FillRect(scoped_refptr<Rect> rect,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void Clear(ExceptionState& exception_state) override;
  void ClearRect(int32_t x,
                 int32_t y,
                 uint32_t width,
                 uint32_t height,
                 ExceptionState& exception_state) override;
  void ClearRect(scoped_refptr<Rect> rect,
                 ExceptionState& exception_state) override;
  scoped_refptr<Color> GetPixel(int32_t x,
                                int32_t y,
                                ExceptionState& exception_state) override;
  void SetPixel(int32_t x,
                int32_t y,
                scoped_refptr<Color> color,
                ExceptionState& exception_state) override;
  void DrawText(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                const std::string& str,
                int32_t align,
                ExceptionState& exception_state) override;
  void DrawText(int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                const std::string& str,
                ExceptionState& exception_state) override;
  void DrawText(scoped_refptr<Rect> rect,
                const std::string& str,
                int32_t align,
                ExceptionState& exception_state) override;
  void DrawText(scoped_refptr<Rect> rect,
                const std::string& str,
                ExceptionState& exception_state) override;
  scoped_refptr<Rect> TextSize(const std::string& str,
                               ExceptionState& exception_state) override;
  std::string DumpData(ExceptionState& exception_state) override;
  void SavePNG(const std::string& filename,
               ExceptionState& exception_state) override;
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Font, scoped_refptr<Font>);

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "Surface"; }

  SDL_Surface* surface_;
  scoped_refptr<FontImpl> font_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_SURFACE_IMPL_H_
