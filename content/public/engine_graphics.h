// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GRAPHICS_H_
#define CONTENT_PUBLIC_ENGINE_GRAPHICS_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_bitmap.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
// Interface referrence: RPGVXAce.chm
/*--urge(name:Graphics,is_module)--*/
class URGE_RUNTIME_API Graphics : public base::RefCounted<Graphics> {
 public:
  virtual ~Graphics() = default;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:wait)--*/
  virtual void Wait(uint32_t duration, ExceptionState& exception_state) = 0;

  /*--urge(name:fade_out)--*/
  virtual void FadeOut(uint32_t duration, ExceptionState& exception_state) = 0;

  /*--urge(name:fade_in)--*/
  virtual void FadeIn(uint32_t duration, ExceptionState& exception_state) = 0;

  /*--urge(name:freeze)--*/
  virtual void Freeze(ExceptionState& exception_state) = 0;

  /*--urge(name:transition)--*/
  virtual void Transition(uint32_t duration,
                          const std::string& filename,
                          uint32_t vague,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:transition_with_bitmap)--*/
  virtual void TransitionWithBitmap(uint32_t duration,
                                    scoped_refptr<Bitmap> bitmap,
                                    uint32_t vague,
                                    ExceptionState& exception_state) = 0;

  /*--urge(name:snap_to_bitmap)--*/
  virtual scoped_refptr<Bitmap> SnapToBitmap(
      ExceptionState& exception_state) = 0;

  /*--urge(name:frame_reset)--*/
  virtual void FrameReset(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual uint32_t Width(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual uint32_t Height(ExceptionState& exception_state) = 0;

  /*--urge(name:resize_screen)--*/
  virtual void ResizeScreen(uint32_t width,
                            uint32_t height,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:play_movie)--*/
  virtual void PlayMovie(const std::string& filename,
                         ExceptionState& exception_state) = 0;

  /*--urge(name:frame_rate)--*/
  URGE_EXPORT_ATTRIBUTE(FrameRate, uint32_t);

  /*--urge(name:frame_count)--*/
  URGE_EXPORT_ATTRIBUTE(FrameCount, uint32_t);

  /*--urge(name:brightness)--*/
  URGE_EXPORT_ATTRIBUTE(Brightness, uint32_t);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GRAPHICS_H_
