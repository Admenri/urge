// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/canvas/animation.h"

#include <cstdint>
#include <string>
#include <vector>

#include "core/common/exception.h"
#include "core/system/filesystem.h"

namespace urge {

static const SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

Animation::Animation(URGE_EXCEPT, std::string filename) {
  IOService::Get().OpenRead(exception_state, filename,
                            [&](SDL_IOStream* stream, const std::string& ext) {
                              animation_ = IMG_LoadAnimationTyped_IO(
                                  stream, true, ext.c_str());
                              return !!animation_;
                            });
}

Animation::~Animation() {
  Disposable::Dispose();
}

int32_t Animation::Width(URGE_EXCEPT) {
  if (!Disposable::Guard(exception_state))
    return {};

  return animation_->w;
}

int32_t Animation::Height(URGE_EXCEPT) {
  if (!Disposable::Guard(exception_state))
    return {};

  return animation_->h;
}

int32_t Animation::Count(URGE_EXCEPT) {
  if (!Disposable::Guard(exception_state))
    return {};

  return animation_->count;
}

int32_t Animation::Delay(URGE_EXCEPT, int32_t index) {
  if (!Disposable::Guard(exception_state))
    return {};

  if (index < 0 || index >= animation_->count)
    return -1;

  return animation_->delays[index];
}

std::vector<int32_t> Animation::Delays(URGE_EXCEPT) {
  if (!Disposable::Guard(exception_state))
    return {};

  return std::vector<int32_t>(animation_->delays,
                              animation_->delays + animation_->count);
}

RefPtr<Image> Animation::Frame(URGE_EXCEPT, int32_t index) {
  if (!Disposable::Guard(exception_state))
    return {};

  if (index < 0 || index >= animation_->count)
    return nullptr;

  auto* frame =
      SDL_ConvertSurface(animation_->frames[index], kInternalPixelFormat);
  return MakeRefCounted<Image>(frame);
}

void Animation::DisposeObject() {
  if (animation_)
    IMG_FreeAnimation(animation_);
  animation_ = nullptr;
}

}
