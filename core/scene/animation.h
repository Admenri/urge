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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SDL3_image/SDL_image.h"

#include "core/definition.h"
#include "core/disposable.h"
#include "core/image.h"
#include "core/refptr.h"

namespace urge {

URGE_BINDING()
class Animation : public Disposable {
 public:
  URGE_BINDING()
  Animation(std::string filename);
  URGE_BINDING()
  ~Animation() override;

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();
  URGE_BINDING()
  int32_t Count();

  URGE_BINDING()
  int32_t Delay(int32_t index);
  URGE_BINDING()
  std::vector<int32_t> Delays();

  URGE_BINDING()
  RefPtr<Image> Frame(int32_t index);

 private:
  void DisposeObject() override;

  IMG_Animation* animation_ = nullptr;
};

}  // namespace urge
