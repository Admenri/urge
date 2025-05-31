// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_ANIMATION_IMPL_H_
#define CONTENT_CANVAS_ANIMATION_IMPL_H_

#include "SDL3_image/SDL_image.h"

#include "content/context/disposable.h"
#include "content/public/engine_imageanimation.h"
#include "content/screen/renderscreen_impl.h"

namespace content {

class ImageAnimationImpl : public ImageAnimation,
                           public GraphicsChild,
                           public Disposable {
 public:
  ImageAnimationImpl(RenderScreenImpl* parent,
                     IMG_Animation* animation,
                     filesystem::IOService* io_service);
  ~ImageAnimationImpl() override;

  ImageAnimationImpl(const ImageAnimationImpl&) = delete;
  ImageAnimationImpl& operator=(const ImageAnimationImpl&) = delete;

 public:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  int32_t Width(ExceptionState& exception_state) override;
  int32_t Height(ExceptionState& exception_state) override;
  base::Vector<scoped_refptr<Surface>> GetFrames(
      ExceptionState& exception_state) override;
  base::Vector<int32_t> GetDelays(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "ImageAnimation"; }

  IMG_Animation* animation_;
  filesystem::IOService* io_service_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_ANIMATION_IMPL_H_
