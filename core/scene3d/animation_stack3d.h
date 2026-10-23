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

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/scene3d/animation3d.h"
#include "core/scene3d/bone_mask3d.h"

namespace urge {

class Skeleton3D;

URGE_BINDING()
class AnimationLayer3D : public Object {
 public:
  AnimationLayer3D(RefPtr<Animation3D> animation, float weight);
  ~AnimationLayer3D() override;

  URGE_BINDING(Name : "animation")
  RefPtr<Animation3D> GetAnimation() const { return animation_; }

  URGE_BINDING(Name : "weight")
  ATTR(float, Weight) {
    if (value.has_value())
      weight_ = (std::max)(0.0f, *value);
    return weight_;
  }

  URGE_BINDING(Name : "speed")
  ATTR(float, Speed) {
    if (value.has_value())
      speed_ = *value;
    return speed_;
  }

  URGE_BINDING(Name : "playing")
  ATTR(bool, Playing) {
    if (value.has_value())
      playing_ = *value;
    return playing_;
  }

  URGE_BINDING(Name : "time")
  ATTR(float, Time) {
    if (value.has_value())
      time_ = *value;
    return time_;
  }

  URGE_BINDING(Name : "mask")
  ATTR(RefPtr<BoneMask3D>, Mask) {
    if (value.has_value())
      mask_ = *value;
    return mask_;
  }

  float weight() const { return weight_; }
  float speed() const { return speed_; }
  bool playing() const { return playing_; }
  float time() const { return time_; }
  RefPtr<Animation3D> animation() const { return animation_; }
  RefPtr<BoneMask3D> mask() const { return mask_; }

 private:
  RefPtr<Animation3D> animation_;
  RefPtr<BoneMask3D> mask_;
  float weight_ = 1.0f;
  float speed_ = 1.0f;
  float time_ = 0.0f;
  bool playing_ = true;
};

URGE_BINDING()
class AnimationStack3D : public Object {
 public:
  URGE_BINDING()
  AnimationStack3D();
  ~AnimationStack3D() override;

  URGE_BINDING()
  RefPtr<AnimationLayer3D> AddLayer(RefPtr<Animation3D> animation,
                                    float weight);
  URGE_BINDING()
  void RemoveLayer(RefPtr<AnimationLayer3D> layer);
  URGE_BINDING()
  void ClearLayers();
  URGE_BINDING()
  int32_t LayerCount() const { return static_cast<int32_t>(layers_.size()); }
  URGE_BINDING()
  RefPtr<AnimationLayer3D> LayerAt(int32_t index) const;

  URGE_BINDING()
  void Update(RefPtr<Skeleton3D> skeleton, float delta);

  URGE_BINDING()
  std::vector<std::string> TakeEvents();
  URGE_BINDING()
  int32_t EventCount() const { return static_cast<int32_t>(events_.size()); }

 private:
  void CollectEvents(AnimationLayer3D* layer, float from, float to);

  std::vector<RefPtr<AnimationLayer3D>> layers_;
  std::vector<std::string> events_;
};

}
