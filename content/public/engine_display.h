// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_DISPLAY_H_
#define CONTENT_PUBLIC_ENGINE_DISPLAY_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:Display)--*/
class URGE_OBJECT(Display) {
 public:
  virtual ~Display() = default;

  /*--urge(name:get_all)--*/
  static base::Vector<scoped_refptr<Display>> GetAll(
      ExecutionContext* execution_context,
      ExceptionState& exception_state);

  /*--urge(name:get_primary)--*/
  static scoped_refptr<Display> GetPrimary(ExecutionContext* execution_context,
                                           ExceptionState& exception_state);

  /*--urge(name:get_from_id)--*/
  static scoped_refptr<Display> GetFromID(ExecutionContext* execution_context,
                                          uint32_t display_id,
                                          ExceptionState& exception_state);

  /*--urge(name:name)--*/
  virtual base::String GetName(ExceptionState& exception_state) = 0;

  /*--urge(name:format)--*/
  virtual base::String GetFormat(ExceptionState& exception_state) = 0;

  /*--urge(name:width)--*/
  virtual int32_t GetWidth(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual int32_t GetHeight(ExceptionState& exception_state) = 0;

  /*--urge(name:content_scale)--*/
  virtual float GetContentScale(ExceptionState& exception_state) = 0;

  /*--urge(name:pixel_density)--*/
  virtual float GetPixelDensity(ExceptionState& exception_state) = 0;

  /*--urge(name:refresh_rate)--*/
  virtual float GetRefreshRate(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_DISPLAY_H_
