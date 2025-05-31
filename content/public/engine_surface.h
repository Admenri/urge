// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_SURFACE_H_
#define CONTENT_PUBLIC_ENGINE_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_color.h"
#include "content/public/engine_iostream.h"
#include "content/public/engine_rect.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:Surface)--*/
class URGE_RUNTIME_API Surface : public base::RefCounted<Surface> {
 public:
  virtual ~Surface() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Surface> New(ExecutionContext* execution_context,
                                    uint32_t width,
                                    uint32_t height,
                                    ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Surface> New(ExecutionContext* execution_context,
                                    const base::String& filename,
                                    ExceptionState& exception_state);

  /*--urge(name:from_dump)--*/
  static scoped_refptr<Surface> FromDump(ExecutionContext* execution_context,
                                         const base::String& dump_data,
                                         ExceptionState& exception_state);

  /*--urge(name:from_stream)--*/
  static scoped_refptr<Surface> FromStream(ExecutionContext* execution_context,
                                           scoped_refptr<IOStream> stream,
                                           const base::String& extname,
                                           ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Surface> Copy(ExecutionContext* execution_context,
                                     scoped_refptr<Surface> other,
                                     ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Surface);

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

  /*--urge(name:blt,optional:opacity=255)--*/
  virtual void Blt(int32_t x,
                   int32_t y,
                   scoped_refptr<Surface> src_palette,
                   scoped_refptr<Rect> src_rect,
                   uint32_t opacity,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:stretch_blt,optional:opacity=255)--*/
  virtual void StretchBlt(scoped_refptr<Rect> dest_rect,
                          scoped_refptr<Surface> src_palette,
                          scoped_refptr<Rect> src_rect,
                          uint32_t opacity,
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

  /*--urge(name:dump_data)--*/
  virtual base::String DumpData(ExceptionState& exception_state) = 0;

  /*--urge(name:save_png)--*/
  virtual void SavePNG(const base::String& filename,
                       ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_SURFACE_H_
