// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_AUDIOSTREAM_H_
#define CONTENT_PUBLIC_ENGINE_AUDIOSTREAM_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:AudioStream)--*/
class URGE_OBJECT(AudioStream) {
 public:
  virtual ~AudioStream() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<AudioStream> New(ExecutionContext* execution_context,
                                        const std::string& filename,
                                        ExceptionState& exception_state);

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:start)--*/
  virtual void Start(ExceptionState& exception_state) = 0;

  /*--urge(name:stop)--*/
  virtual void Stop(ExceptionState& exception_state) = 0;

  /*--urge(name:seek)--*/
  virtual void Seek(float time, ExceptionState& exception_state) = 0;

  /*--urge(name:cursor)--*/
  virtual float Cursor(ExceptionState& exception_state) = 0;

  /*--urge(name:length)--*/
  virtual float Length(ExceptionState& exception_state) = 0;

  /*--urge(name:playing?)--*/
  virtual bool IsPlaying(ExceptionState& exception_state) = 0;

  /*--urge(name:end?)--*/
  virtual bool AtEnd(ExceptionState& exception_state) = 0;

  /*--urge(name:set_start_time)--*/
  virtual void SetStartTime(uint64_t time, ExceptionState& exception_state) = 0;

  /*--urge(name:set_stop_time)--*/
  virtual void SetStopTime(uint64_t time, ExceptionState& exception_state) = 0;

  /*--urge(name:volume)--*/
  URGE_EXPORT_ATTRIBUTE(Volume, float);

  /*--urge(name:pan)--*/
  URGE_EXPORT_ATTRIBUTE(Pan, float);

  /*--urge(name:pitch)--*/
  URGE_EXPORT_ATTRIBUTE(Pitch, float);

  /*--urge(name:loop)--*/
  URGE_EXPORT_ATTRIBUTE(Loop, bool);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_AUDIOSTREAM_H_
