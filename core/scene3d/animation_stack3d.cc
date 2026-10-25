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

#include "core/scene3d/animation_stack3d.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/common/exception.h"
#include "core/scene3d/skeleton3d.h"

namespace urge {

AnimationLayer3D::AnimationLayer3D(RefPtr<Animation3D> animation, float weight)
    : animation_(std::move(animation)), weight_(std::max(0.0f, weight)) {
  if (!animation_)
    throw Exception(Exception::kRGSSError,
                    "animation layer 3d: an animation is needed.");
}

AnimationLayer3D::~AnimationLayer3D() = default;

AnimationStack3D::AnimationStack3D() = default;

AnimationStack3D::~AnimationStack3D() = default;

RefPtr<AnimationLayer3D> AnimationStack3D::AddLayer(
    RefPtr<Animation3D> animation,
    float weight) {
  auto layer = MakeRefCounted<AnimationLayer3D>(std::move(animation), weight);
  layers_.push_back(layer);
  return layer;
}

void AnimationStack3D::RemoveLayer(RefPtr<AnimationLayer3D> layer) {
  if (!layer)
    return;
  layers_.erase(std::remove(layers_.begin(), layers_.end(), layer),
                layers_.end());
}

void AnimationStack3D::ClearLayers() {
  layers_.clear();
}

RefPtr<AnimationLayer3D> AnimationStack3D::LayerAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(layers_.size()))
    return nullptr;
  return layers_[index];
}

void AnimationStack3D::Update(RefPtr<Skeleton3D> skeleton, float delta) {
  skeleton->BeginPose();

  for (const RefPtr<AnimationLayer3D>& layer : layers_) {
    if (!layer->playing() || layer->weight() <= 0.0f)
      continue;

    RefPtr<Animation3D> animation = layer->animation();
    if (!animation)
      continue;

    const float from = layer->time();
    float to = from + delta * layer->speed();

    const float duration = animation->Duration();
    if (animation->loop() && duration > 0.0f) {
      to = std::fmod(to, duration);
      if (to < 0.0f)
        to += duration;
    }

    animation->CollectEvents(from, to, events_);
    layer->Attr_Time(to);

    if (layer->mask())
      animation->BlendMasked(skeleton, to, layer->weight(), layer->mask());
    else
      animation->Blend(skeleton, to, layer->weight());
  }

  skeleton->EndPose();
}

std::vector<std::string> AnimationStack3D::TakeEvents() {
  std::vector<std::string> result;
  result.swap(events_);
  return result;
}

}
