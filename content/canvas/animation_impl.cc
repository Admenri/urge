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
    const std::string& filename,
    ExceptionState& exception_state) {
  IMG_Animation* animation_data = nullptr;
  auto file_handler = base::BindRepeating(
      [](IMG_Animation** anim, SDL_IOStream* ops, const std::string& ext) {
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

  return base::MakeRefCounted<ImageAnimationImpl>(
      execution_context->graphics, animation_data,
      execution_context->io_service);
}

scoped_refptr<ImageAnimation> ImageAnimation::New(
    ExecutionContext* execution_context,
    scoped_refptr<IOStream> stream,
    const std::string& extname,
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

  return base::MakeRefCounted<ImageAnimationImpl>(
      execution_context->graphics, memory_animation,
      execution_context->io_service);
}

ImageAnimationImpl::ImageAnimationImpl(RenderScreenImpl* parent,
                                       IMG_Animation* animation,
                                       filesystem::IOService* io_service)
    : GraphicsChild(parent),
      Disposable(parent),
      animation_(animation),
      io_service_(io_service) {}

ImageAnimationImpl::~ImageAnimationImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void ImageAnimationImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool ImageAnimationImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

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

std::vector<scoped_refptr<Surface>> ImageAnimationImpl::GetFrames(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return {};

  std::vector<scoped_refptr<Surface>> result;
  for (int32_t i = 0; i < animation_->count; ++i) {
    auto* origin_surface = animation_->frames[i];
    auto* duplicate_surface = SDL_CreateSurface(
        origin_surface->w, origin_surface->h, origin_surface->format);
    std::memcpy(duplicate_surface->pixels, origin_surface->pixels,
                origin_surface->pitch * origin_surface->h);

    result.push_back(base::MakeRefCounted<SurfaceImpl>(
        screen(), duplicate_surface, io_service_));
  }

  return result;
}

std::vector<int32_t> ImageAnimationImpl::GetDelays(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return {};

  std::vector<int32_t> result;
  for (int32_t i = 0; i < animation_->count; ++i)
    result.push_back(animation_->delays[i]);

  return result;
}

void ImageAnimationImpl::OnObjectDisposed() {
  IMG_FreeAnimation(animation_);
  animation_ = nullptr;
}

}  // namespace content
