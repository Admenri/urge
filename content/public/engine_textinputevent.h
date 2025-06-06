// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_TEXTINPUTEVENT_H_
#define CONTENT_PUBLIC_ENGINE_TEXTINPUTEVENT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_rect.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:TextInputEvent)--*/
class URGE_RUNTIME_API TextInputEvent
    : public base::RefCounted<TextInputEvent> {
 public:
  virtual ~TextInputEvent() = default;

  /*--urge(name:EventType)--*/
  enum Type {
    TYPE_TEXT_EDITING = 0,
    TYPE_TEXT_INPUT,
  };

  /*--urge(name:update)--*/
  static base::Vector<scoped_refptr<TextInputEvent>> Update(
      ExecutionContext* execution_context,
      ExceptionState& exception_state);

  /*--urge(name:enable)--*/
  static bool Enable(ExecutionContext* execution_context,
                     scoped_refptr<Rect> region,
                     ExceptionState& exception_state);

  /*--urge(name:active?)--*/
  static bool IsActivated(ExecutionContext* execution_context,
                          ExceptionState& exception_state);

  /*--urge(name:disable)--*/
  static bool Disable(ExecutionContext* execution_context,
                      ExceptionState& exception_state);

  /*--urge(name:type)--*/
  virtual Type GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:text)--*/
  virtual base::String GetText(ExceptionState& exception_state) = 0;

  /*--urge(name:editing_start)--*/
  virtual int32_t GetEditingStart(ExceptionState& exception_state) = 0;

  /*--urge(name:editing_length)--*/
  virtual int32_t GetEditingLength(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_TEXTINPUTEVENT_H_
