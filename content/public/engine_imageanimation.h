// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_IMAGEANIMATION_H_
#define CONTENT_PUBLIC_ENGINE_IMAGEANIMATION_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_iostream.h"
#include "content/public/engine_surface.h"

namespace content {

/*--urge(name:ImageAnimation)--*/
class URGE_OBJECT(ImageAnimation) {
 public:
  virtual ~ImageAnimation() = default;

  /*--urge(name:initialize)--*/
  static scoped_refptr<ImageAnimation> New(ExecutionContext* execution_context,
                                           const std::string& filename,
                                           ExceptionState& exception_state);

  /*--urge(name:initialize)--*/
  static scoped_refptr<ImageAnimation> New(ExecutionContext* execution_context,
                                           scoped_refptr<IOStream> stream,
                                           const std::string& extname,
                                           ExceptionState& exception_state);

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:width)--*/
  virtual int32_t Width(ExceptionState& exception_state) = 0;

  /*--urge(name:height)--*/
  virtual int32_t Height(ExceptionState& exception_state) = 0;

  /*--urge(name:frames)--*/
  virtual std::vector<scoped_refptr<Surface>> GetFrames(
      ExceptionState& exception_state) = 0;

  /*--urge(name:delays)--*/
  virtual std::vector<int32_t> GetDelays(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_IMAGEANIMATION_H_
