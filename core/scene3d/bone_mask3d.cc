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

#include "core/scene3d/bone_mask3d.h"

#include <algorithm>

namespace urge {

BoneMask3D::BoneMask3D() = default;

BoneMask3D::~BoneMask3D() = default;

void BoneMask3D::Resize(int32_t count) {
  weights_.assign(static_cast<size_t>(std::max(0, count)), 1.0f);
}

void BoneMask3D::Fill(float weight) {
  std::fill(weights_.begin(), weights_.end(), std::clamp(weight, 0.0f, 1.0f));
}

void BoneMask3D::Set(int32_t index, float weight) {
  if (index < 0 || index >= static_cast<int32_t>(weights_.size()))
    return;
  weights_[index] = std::clamp(weight, 0.0f, 1.0f);
}

float BoneMask3D::Get(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(weights_.size()))
    return 0.0f;
  return weights_[index];
}

}
