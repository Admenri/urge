// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_BITMAP_H_
#define CONTENT_PUBLIC_ENGINE_BITMAP_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_color.h"
#include "content/public/engine_font.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gputexture.h"
#include "content/public/engine_iostream.h"
#include "content/public/engine_rect.h"
#include "content/public/engine_surface.h"

namespace content {

/*--urge(name:Bitmap)--*/
class URGE_OBJECT(Bitmap) {
 public:
  virtual ~Bitmap() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                   const std::string& filename,
                                   ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Bitmap> New(ExecutionContext* execution_context,
                                   uint32_t width,
                                   uint32_t height,
                                   ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Bitmap> Copy(ExecutionContext* execution_context,
                                    scoped_refptr<Bitmap> other,
                                    ExceptionState& exception_state);

  /*--urge(name:from_surface)--*/
  static scoped_refptr<Bitmap> FromSurface(ExecutionContext* execution_context,
                                           scoped_refptr<Surface> surface,
                                           ExceptionState& exception_state);

  /*--urge(name:from_texture)--*/
  static scoped_refptr<Bitmap> FromTexture(
      ExecutionContext* execution_context,
      scoped_refptr<GPUTexture> gpu_texture,
      ExceptionState& exception_state);

  /*--urge(name:from_stream)--*/
  static scoped_refptr<Bitmap> FromStream(ExecutionContext* execution_context,
                                          scoped_refptr<IOStream> stream,
                                          const std::string& extname,
                                          ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Bitmap);

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual uint32_t Width(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual uint32_t Height(ExceptionState& exception_state) = 0;

  /*--urge(name:rect)--*/
  virtual scoped_refptr<Rect> GetRect(ExceptionState& exception_state) = 0;

  /*--urge(name:blt,optional:opacity=255,optional:blend_type=-1)--*/
  virtual void Blt(int32_t x,
                   int32_t y,
                   scoped_refptr<Bitmap> src_bitmap,
                   scoped_refptr<Rect> src_rect,
                   uint32_t opacity,
                   int32_t blend_type,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:stretch_blt,optional:opacity=255,optional:blend_type=-1)--*/
  virtual void StretchBlt(scoped_refptr<Rect> dest_rect,
                          scoped_refptr<Bitmap> src_bitmap,
                          scoped_refptr<Rect> src_rect,
                          uint32_t opacity,
                          int32_t blend_type,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:clip_blt)--*/
  virtual void ClipBlt(scoped_refptr<Rect> dest_rect,
                       scoped_refptr<Bitmap> src_bitmap,
                       scoped_refptr<Rect> src_rect,
                       scoped_refptr<Bitmap> clip_bitmap,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:fill_rect)--*/
  virtual void FillRect(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:fill_rect)--*/
  virtual void FillRect(scoped_refptr<Rect> rect,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(int32_t x,
                                int32_t y,
                                uint32_t width,
                                uint32_t height,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                bool vertical,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(int32_t x,
                                int32_t y,
                                uint32_t width,
                                uint32_t height,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(scoped_refptr<Rect> rect,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                bool vertical,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:gradient_fill_rect)--*/
  virtual void GradientFillRect(scoped_refptr<Rect> rect,
                                scoped_refptr<Color> color1,
                                scoped_refptr<Color> color2,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:clear)--*/
  virtual void Clear(ExceptionState& exception_state) = 0;

  /*--urge(name:clear_rect)--*/
  virtual void ClearRect(int32_t x,
                         int32_t y,
                         uint32_t width,
                         uint32_t height,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:clear_rect)--*/
  virtual void ClearRect(scoped_refptr<Rect> rect,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:get_pixel)--*/
  virtual scoped_refptr<Color> GetPixel(int32_t x,
                                        int32_t y,
                                        ExceptionState& exception_state) = 0;

  /*--urge(name:set_pixel)--*/
  virtual void SetPixel(int32_t x,
                        int32_t y,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:hue_change)--*/
  virtual void HueChange(int32_t hue, ExceptionState& exception_state) = 0;

  /*--urge(name:blur)--*/
  virtual void Blur(ExceptionState& exception_state) = 0;

  /*--urge(name:radial_blur)--*/
  virtual void RadialBlur(int32_t angle,
                          int32_t division,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        const std::string& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(int32_t x,
                        int32_t y,
                        uint32_t width,
                        uint32_t height,
                        const std::string& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const std::string& str,
                        int32_t align,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:draw_text)--*/
  virtual void DrawText(scoped_refptr<Rect> rect,
                        const std::string& str,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:text_size)--*/
  virtual scoped_refptr<Rect> TextSize(const std::string& str,
                                       ExceptionState& exception_state) = 0;

  /*--urge(name:create_surface)--*/
  virtual scoped_refptr<Surface> CreateSurface(
      ExceptionState& exception_state) = 0;

  /*--urge(name:texture)--*/
  virtual scoped_refptr<GPUTexture> GetTexture(
      ExceptionState& exception_state) = 0;

  /*--urge(name:depth_stencil)--*/
  virtual scoped_refptr<GPUTexture> GetDepthStencil(
      ExceptionState& exception_state) = 0;

  /*--urge(name:shader_resource_view)--*/
  virtual scoped_refptr<GPUTextureView> GetShaderResourceView(
      ExceptionState& exception_state) = 0;

  /*--urge(name:render_target_view)--*/
  virtual scoped_refptr<GPUTextureView> GetRenderTargetView(
      ExceptionState& exception_state) = 0;

  /*--urge(name:depth_stencil_view)--*/
  virtual scoped_refptr<GPUTextureView> GetDepthStencilView(
      ExceptionState& exception_state) = 0;

  /*--urge(name:world_uniform_buffer)--*/
  virtual scoped_refptr<GPUBuffer> GetWorldUniformBuffer(
      ExceptionState& exception_state) = 0;

  /*--urge(name:font)--*/
  URGE_EXPORT_ATTRIBUTE(Font, scoped_refptr<Font>);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_BITMAP_H_
