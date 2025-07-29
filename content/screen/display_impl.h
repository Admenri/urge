// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SCREEN_DISPLAY_IMPL_H_
#define CONTENT_SCREEN_DISPLAY_IMPL_H_

#include "SDL3/SDL_video.h"

#include "content/public/engine_display.h"

namespace content {

class DisplayImpl : public Display {
 public:
  DisplayImpl(SDL_DisplayID display);
  ~DisplayImpl() override;

  DisplayImpl(const DisplayImpl&) = delete;
  DisplayImpl& operator=(const DisplayImpl&) = delete;

 public:
  std::string GetName(ExceptionState& exception_state) override;
  std::string GetFormat(ExceptionState& exception_state) override;
  int32_t GetWidth(ExceptionState& exception_state) override;
  int32_t GetHeight(ExceptionState& exception_state) override;
  float GetContentScale(ExceptionState& exception_state) override;
  float GetPixelDensity(ExceptionState& exception_state) override;
  float GetRefreshRate(ExceptionState& exception_state) override;

 private:
  SDL_DisplayID display_;
  const SDL_DisplayMode* mode_;
};

}  // namespace content

#endif  //! CONTENT_SCREEN_DISPLAY_IMPL_H_
