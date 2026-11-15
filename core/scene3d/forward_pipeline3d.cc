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

#include "core/scene3d/forward_pipeline3d.h"

#include <algorithm>

namespace urge {

ForwardPipeline3D::ForwardPipeline3D(URGE_EXCEPT) {
  Attr_ShadowSize(1024);

  const auto shadow_pass = AddPass(exception_state, "shadow");
  shadow_pass->Attr_DepthOnly(true);
  shadow_pass->Attr_Cascades(4);
  shadow_pass->SetColors({"shadow"});
  shadow_pass->Attr_ClearFlags(3u);
  shadow_pass->Attr_QueueMin(0);
  shadow_pass->Attr_QueueMax(4000);
  shadow_pass->Attr_Layers(0xFFFFFFFFu);

  const auto opaque_pass = AddPass(exception_state, "opaque");
  opaque_pass->SetTags({"opaque"});
  opaque_pass->Attr_ClearFlags(3u);
  opaque_pass->Attr_QueueMin(0);
  opaque_pass->Attr_QueueMax(queue_split_ - 1);

  const auto transparent_back_pass =
      AddPass(exception_state, "transparent_back");
  transparent_back_pass->SetTags({"transparent"});
  transparent_back_pass->Attr_ClearFlags(0u);
  transparent_back_pass->Attr_QueueMin(queue_split_);
  transparent_back_pass->Attr_QueueMax(4000);
  transparent_back_pass->Attr_SortBackToFront(true);
  transparent_back_pass->Attr_CullOverride(2);

  const auto transparent_pass = AddPass(exception_state, "transparent");
  transparent_pass->SetTags({"transparent"});
  transparent_pass->Attr_ClearFlags(0u);
  transparent_pass->Attr_QueueMin(queue_split_);
  transparent_pass->Attr_QueueMax(4000);
  transparent_pass->Attr_SortBackToFront(true);
  transparent_pass->Attr_CullOverride(3);
}

ForwardPipeline3D::~ForwardPipeline3D() = default;

void ForwardPipeline3D::SetCascades(int32_t count) {
  if (const auto pass = GetPass("shadow"))
    pass->Attr_Cascades(count);
}

void ForwardPipeline3D::SetShadowDistance(float distance) {
  if (const auto pass = GetPass("shadow"))
    pass->Attr_ShadowDistance(distance);
}

void ForwardPipeline3D::SetSplitLambda(float lambda) {
  if (const auto pass = GetPass("shadow"))
    pass->Attr_SplitLambda(lambda);
}

void ForwardPipeline3D::SetCascadeFade(float fade) {
  if (const auto pass = GetPass("shadow"))
    pass->Attr_CascadeFade(fade);
}

void ForwardPipeline3D::SetShadowEnabled(bool enabled) {
  if (const auto pass = GetPass("shadow"))
    pass->Attr_Enabled(enabled);
}

void ForwardPipeline3D::SetQueueSplit(int32_t queue) {
  queue_split_ = std::max(1, queue);
  if (const auto pass = GetPass("opaque"))
    pass->Attr_QueueMax(queue_split_ - 1);
  if (const auto pass = GetPass("transparent"))
    pass->Attr_QueueMin(queue_split_);
  if (const auto pass = GetPass("transparent_back"))
    pass->Attr_QueueMin(queue_split_);
}

}
