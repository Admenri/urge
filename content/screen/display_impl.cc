// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/display_impl.h"

namespace content {

// static
std::vector<scoped_refptr<Display>> Display::GetAll(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  int32_t count;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);

  std::vector<scoped_refptr<Display>> result;
  for (int32_t i = 0; i < count; ++i)
    result.push_back(base::MakeRefCounted<DisplayImpl>(displays[i]));

  return result;
}

// static
scoped_refptr<Display> Display::GetPrimary(ExecutionContext* execution_context,
                                           ExceptionState& exception_state) {
  return base::MakeRefCounted<DisplayImpl>(SDL_GetPrimaryDisplay());
}

// static
scoped_refptr<Display> Display::GetFromID(ExecutionContext* execution_context,
                                          uint32_t display_id,
                                          ExceptionState& exception_state) {
  return base::MakeRefCounted<DisplayImpl>(display_id);
}

///////////////////////////////////////////////////////////////////////////////
// DisplayImpl Implement

DisplayImpl::DisplayImpl(SDL_DisplayID display)
    : display_(display), mode_(SDL_GetDesktopDisplayMode(display)) {}

DisplayImpl::~DisplayImpl() = default;

std::string DisplayImpl::GetName(ExceptionState& exception_state) {
  return SDL_GetDisplayName(display_);
}

std::string DisplayImpl::GetFormat(ExceptionState& exception_state) {
  return SDL_GetPixelFormatName(mode_->format);
}

int32_t DisplayImpl::GetWidth(ExceptionState& exception_state) {
  return mode_->w;
}

int32_t DisplayImpl::GetHeight(ExceptionState& exception_state) {
  return mode_->h;
}

float DisplayImpl::GetContentScale(ExceptionState& exception_state) {
  return SDL_GetDisplayContentScale(display_);
}

float DisplayImpl::GetPixelDensity(ExceptionState& exception_state) {
  return mode_->pixel_density;
}

float DisplayImpl::GetRefreshRate(ExceptionState& exception_state) {
  return mode_->refresh_rate;
}

}  // namespace content
