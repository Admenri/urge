// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_VIDEODECODER_H_
#define CONTENT_PUBLIC_ENGINE_VIDEODECODER_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_bitmap.h"
#include "content/public/engine_iostream.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:VideoDecoder)--*/
class URGE_RUNTIME_API VideoDecoder : public base::RefCounted<VideoDecoder> {
 public:
  virtual ~VideoDecoder() = default;

  /*--urge(name:PlayState)--*/
  enum PlayState {
    STATE_PLAYING = 0,
    STATE_PAUSED,
    STATE_STOPPED,
    STATE_FINISHED,
  };

  /*--urge(name:initialize)--*/
  static scoped_refptr<VideoDecoder> New(ExecutionContext* execution_context,
                                         const base::String& filename,
                                         ExceptionState& exception_state);

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual int32_t GetWidth(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual int32_t GetHeight(ExceptionState& exception_state) = 0;

  /*--urge(name:duration)--*/
  virtual float GetDuration(ExceptionState& exception_state) = 0;

  /*--urge(name:frame_rate)--*/
  virtual float GetFrameRate(ExceptionState& exception_state) = 0;

  /*--urge(name:has_audio?)--*/
  virtual bool HasAudio(ExceptionState& exception_state) = 0;

  /*--urge(name:update)--*/
  virtual void Update(ExceptionState& exception_state) = 0;

  /*--urge(name:render)--*/
  virtual void Render(scoped_refptr<Bitmap> target,
                      ExceptionState& exception_state) = 0;

  /*--urge(name:state)--*/
  URGE_EXPORT_ATTRIBUTE(State, PlayState);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_VIDEODECODER_H_
