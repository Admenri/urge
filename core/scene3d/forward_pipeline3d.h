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

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/scene3d/render_graph3d.h"

namespace urge {

URGE_BINDING()
class ForwardPipeline3D : public RenderGraph3D {
 public:
  URGE_BINDING()
  ForwardPipeline3D();
  ~ForwardPipeline3D() override;

  URGE_BINDING()
  void SetCascades(int32_t count);
  URGE_BINDING()
  void SetShadowDistance(float distance);
  URGE_BINDING()
  void SetSplitLambda(float lambda);
  URGE_BINDING()
  void SetCascadeFade(float fade);
  URGE_BINDING()
  void SetShadowEnabled(bool enabled);
  URGE_BINDING()
  void SetQueueSplit(int32_t queue);

 private:
  RefPtr<RenderGraphPass3D> shadow_pass_;
  RefPtr<RenderGraphPass3D> opaque_pass_;
  RefPtr<RenderGraphPass3D> transparent_back_pass_;
  RefPtr<RenderGraphPass3D> transparent_pass_;
  int32_t queue_split_ = 2500;
};

}
