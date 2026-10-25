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
#include <vector>

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/scene3d/bone_mask3d.h"

namespace urge {

class Skeleton3D;

URGE_BINDING()
class Animation3D : public Object {
 public:
  enum Path {
    kTranslation = 0,
    kRotation = 1,
    kScale = 2,
  };

  enum Interpolation {
    kLinear = 0,
    kStep = 1,
  };

  URGE_BINDING()
  Animation3D();
  ~Animation3D() override;

  URGE_BINDING()
  void AddTrack(int32_t bone,
                int32_t path,
                std::vector<float> times,
                std::vector<float> values,
                int32_t interpolation);

  URGE_BINDING()
  void AddEvent(float time, std::string name);
  URGE_BINDING()
  int32_t EventCount() const { return static_cast<int32_t>(events_.size()); }
  URGE_BINDING()
  std::string EventName(int32_t index) const;
  URGE_BINDING()
  float EventTime(int32_t index) const;

  struct Event {
    float time = 0.0f;
    std::string name;
  };

  const std::vector<Event>& events() const { return events_; }

  void CollectEvents(float from, float to, std::vector<std::string>& out) const;

  URGE_BINDING()
  float Duration() const { return duration_; }
  URGE_BINDING()
  int32_t TrackCount() const { return static_cast<int32_t>(tracks_.size()); }

  URGE_BINDING(Name : "loop")
  ATTR(bool, Loop) {
    if (value.has_value())
      loop_ = *value;
    return loop_;
  }

  URGE_BINDING(Name : "speed")
  ATTR(float, Speed) {
    if (value.has_value())
      speed_ = *value;
    return speed_;
  }

  URGE_BINDING()
  void Apply(RefPtr<Skeleton3D> skeleton, float time);
  URGE_BINDING()
  void Blend(RefPtr<Skeleton3D> skeleton, float time, float weight);
  URGE_BINDING()
  void BlendMasked(RefPtr<Skeleton3D> skeleton,
                   float time,
                   float weight,
                   MaybePtr<BoneMask3D> mask);

  bool loop() const { return loop_; }
  float speed() const { return speed_; }

 private:
  struct Track {
    int32_t bone = 0;
    int32_t path = kTranslation;
    int32_t stride = 3;
    int32_t interpolation = kLinear;
    std::vector<float> times;
    std::vector<float> values;
  };

  void Sample(Skeleton3D* skeleton,
              float time,
              float weight,
              const std::vector<float>* mask);

  float duration_ = 0.0f;
  bool loop_ = true;
  float speed_ = 1.0f;
  std::vector<Track> tracks_;
  std::vector<Event> events_;
};

}
