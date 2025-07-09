// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_TONE_H_
#define CONTENT_PUBLIC_ENGINE_TONE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:Tone)--*/
class URGE_OBJECT(Tone) {
 public:
  virtual ~Tone() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Tone> New(ExecutionContext* execution_context,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Tone> New(ExecutionContext* execution_context,
                                 float red,
                                 float green,
                                 float blue,
                                 float gray,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<Tone> New(ExecutionContext* execution_context,
                                 float red,
                                 float green,
                                 float blue,
                                 ExceptionState& exception_state);

  /*--urge(name:initialize_copy)--*/
  static scoped_refptr<Tone> Copy(ExecutionContext* execution_context,
                                  scoped_refptr<Tone> other,
                                  ExceptionState& exception_state);

  /*--urge(serializable)--*/
  URGE_EXPORT_SERIALIZABLE(Tone);

  /*--urge(comparable)--*/
  URGE_EXPORT_COMPARABLE(Tone);

  /*--urge(name:set)--*/
  virtual void Set(float red,
                   float green,
                   float blue,
                   float gray,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:set)--*/
  virtual void Set(float red,
                   float green,
                   float blue,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:set)--*/
  virtual void Set(scoped_refptr<Tone> other,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:red)--*/
  URGE_EXPORT_ATTRIBUTE(Red, float);

  /*--urge(name:green)--*/
  URGE_EXPORT_ATTRIBUTE(Green, float);

  /*--urge(name:blue)--*/
  URGE_EXPORT_ATTRIBUTE(Blue, float);

  /*--urge(name:gray)--*/
  URGE_EXPORT_ATTRIBUTE(Gray, float);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_TONE_H_
