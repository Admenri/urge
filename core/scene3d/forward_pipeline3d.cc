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

ForwardPipeline3D::ForwardPipeline3D() {
  Attr_ShadowSize(1024);

  shadow_pass_ = AddPass("shadow");
  shadow_pass_->Attr_DepthOnly(true);
  shadow_pass_->Attr_Cascades(4);
  shadow_pass_->SetColors({"shadow"});
  shadow_pass_->Attr_ClearFlags(3u);
  shadow_pass_->Attr_QueueMin(0);
  shadow_pass_->Attr_QueueMax(4000);
  shadow_pass_->Attr_Layers(0xFFFFFFFFu);

  opaque_pass_ = AddPass("opaque");
  opaque_pass_->SetTags({"opaque"});
  opaque_pass_->Attr_ClearFlags(3u);
  opaque_pass_->Attr_QueueMin(0);
  opaque_pass_->Attr_QueueMax(queue_split_ - 1);

  transparent_back_pass_ = AddPass("transparent_back");
  transparent_back_pass_->SetTags({"transparent"});
  transparent_back_pass_->Attr_ClearFlags(0u);
  transparent_back_pass_->Attr_QueueMin(queue_split_);
  transparent_back_pass_->Attr_QueueMax(4000);
  transparent_back_pass_->Attr_SortBackToFront(true);
  transparent_back_pass_->Attr_CullOverride(2);

  transparent_pass_ = AddPass("transparent");
  transparent_pass_->SetTags({"transparent"});
  transparent_pass_->Attr_ClearFlags(0u);
  transparent_pass_->Attr_QueueMin(queue_split_);
  transparent_pass_->Attr_QueueMax(4000);
  transparent_pass_->Attr_SortBackToFront(true);
  transparent_pass_->Attr_CullOverride(3);
}

ForwardPipeline3D::~ForwardPipeline3D() = default;

void ForwardPipeline3D::SetCascades(int32_t count) {
  if (shadow_pass_)
    shadow_pass_->Attr_Cascades(count);
}

void ForwardPipeline3D::SetShadowDistance(float distance) {
  if (shadow_pass_)
    shadow_pass_->Attr_ShadowDistance(distance);
}

void ForwardPipeline3D::SetSplitLambda(float lambda) {
  if (shadow_pass_)
    shadow_pass_->Attr_SplitLambda(lambda);
}

void ForwardPipeline3D::SetCascadeFade(float fade) {
  if (shadow_pass_)
    shadow_pass_->Attr_CascadeFade(fade);
}

void ForwardPipeline3D::SetShadowEnabled(bool enabled) {
  if (shadow_pass_)
    shadow_pass_->Attr_Enabled(enabled);
}

void ForwardPipeline3D::SetQueueSplit(int32_t queue) {
  queue_split_ = std::max(1, queue);
  if (opaque_pass_)
    opaque_pass_->Attr_QueueMax(queue_split_ - 1);
  if (transparent_pass_)
    transparent_pass_->Attr_QueueMin(queue_split_);
  if (transparent_back_pass_)
    transparent_back_pass_->Attr_QueueMin(queue_split_);
}

}
