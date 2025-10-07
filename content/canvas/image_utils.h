// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_IMAGE_UTILS_H_
#define CONTENT_CANVAS_IMAGE_UTILS_H_

#include "SDL3/SDL_surface.h"

namespace content {

// aka RGBA32
constexpr SDL_PixelFormat kSurfaceInternalFormat = SDL_PIXELFORMAT_ABGR8888;

// Convert target surface to engine internal expected format.
// The converting will destroy origin surface.
inline void MakeSureSurfaceFormat(SDL_Surface*& surface) {
  if (surface->format != kSurfaceInternalFormat) {
    auto* converted_surface =
        SDL_ConvertSurface(surface, kSurfaceInternalFormat);
    SDL_DestroySurface(surface);
    surface = converted_surface;
  }
}

}  // namespace content

#endif  //! CONTENT_CANVAS_IMAGE_UTILS_H_
