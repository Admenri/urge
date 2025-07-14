// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/animation_impl.h"

#include "components/filesystem/io_service.h"
#include "content/canvas/surface_impl.h"
#include "content/io/iostream_impl.h"

namespace content {

scoped_refptr<ImageAnimation> ImageAnimation::New(
    ExecutionContext* execution_context,
    const base::String& filename,
    ExceptionState& exception_state) {
  IMG_Animation* animation_data = nullptr;
  auto file_handler = base::BindRepeating(
      [](IMG_Animation** anim, SDL_IOStream* ops, const base::String& ext) {
        *anim = IMG_LoadAnimationTyped_IO(ops, true, ext.c_str());
        return !!*anim;
      },
      &animation_data);

  filesystem::IOState io_state;
  execution_context->io_service->OpenRead(filename, file_handler, &io_state);

  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::IO_ERROR,
                               "Failed to read file: %s (%s)", filename.c_str(),
                               io_state.error_message.c_str());
    return nullptr;
  }

  if (!animation_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load animation image: %s (%s)",
                               filename.c_str(), SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<ImageAnimationImpl>(execution_context,
                                                  animation_data);
}

scoped_refptr<ImageAnimation> ImageAnimation::New(
    ExecutionContext* execution_context,
    scoped_refptr<IOStream> stream,
    const base::String& extname,
    ExceptionState& exception_state) {
  auto stream_obj = IOStreamImpl::From(stream);
  if (!stream_obj || !stream_obj->GetRawStream()) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid iostream input.");
    return nullptr;
  }

  IMG_Animation* memory_animation = IMG_LoadAnimationTyped_IO(
      stream_obj->GetRawStream(), false, extname.c_str());
  if (!memory_animation) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load animation from iostream. (%s)",
                               SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<ImageAnimationImpl>(execution_context,
                                                  memory_animation);
}

ImageAnimationImpl::ImageAnimationImpl(ExecutionContext* execution_context,
                                       IMG_Animation* animation)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      animation_(animation) {}

DISPOSABLE_DEFINITION(ImageAnimationImpl);

int32_t ImageAnimationImpl::Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return animation_->w;
}

int32_t ImageAnimationImpl::Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return animation_->h;
}

base::Vector<scoped_refptr<Surface>> ImageAnimationImpl::GetFrames(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return {};

  base::Vector<scoped_refptr<Surface>> result;
  for (int32_t i = 0; i < animation_->count; ++i) {
    auto* origin_surface = animation_->frames[i];
    auto* duplicate_surface = SDL_CreateSurface(
        origin_surface->w, origin_surface->h, origin_surface->format);
    std::memcpy(duplicate_surface->pixels, origin_surface->pixels,
                origin_surface->pitch * origin_surface->h);

    result.push_back(
        base::MakeRefCounted<SurfaceImpl>(context(), duplicate_surface));
  }

  return result;
}

base::Vector<int32_t> ImageAnimationImpl::GetDelays(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return {};

  base::Vector<int32_t> result;
  for (int32_t i = 0; i < animation_->count; ++i)
    result.push_back(animation_->delays[i]);

  return result;
}

void ImageAnimationImpl::OnObjectDisposed() {
  IMG_FreeAnimation(animation_);
  animation_ = nullptr;
}

}  // namespace content
