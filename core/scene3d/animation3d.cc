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

#include "core/scene3d/animation3d.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/common/exception.h"
#include "core/scene3d/skeleton3d.h"

namespace urge {

namespace {

size_t FindKey(const std::vector<float>& times, float time) {
  const auto it = std::upper_bound(times.begin(), times.end(), time);
  if (it == times.begin())
    return 0;
  return static_cast<size_t>(it - times.begin() - 1);
}

}

Animation3D::Animation3D() = default;

Animation3D::~Animation3D() = default;

void Animation3D::AddTrack(URGE_EXCEPT,
                           int32_t bone,
                           int32_t path,
                           std::vector<float> times,
                           std::vector<float> values,
                           int32_t interpolation) {
  if (path != kTranslation && path != kRotation && path != kScale) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "animation3d: unknown channel path {}.", path);
    return;
  }
  const int32_t stride = path == kRotation ? 4 : 3;
  if (times.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "animation3d: a track needs at least one key.");
    return;
  }
  if (values.size() != times.size() * static_cast<size_t>(stride)) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "animation3d: the value count does not match the key "
                          "count.");
    return;
  }
  Track track;
  track.bone = bone;
  track.path = path;
  track.stride = stride;
  track.interpolation = interpolation == kStep ? kStep : kLinear;
  track.times = std::move(times);
  track.values = std::move(values);

  duration_ = std::max(duration_, track.times.back());
  const auto position = std::upper_bound(
      tracks_.begin(), tracks_.end(), bone,
      [](int32_t value, const Track& item) { return value < item.bone; });
  tracks_.insert(position, std::move(track));
}

void Animation3D::AddEvent(URGE_EXCEPT, float time, std::string name) {
  if (name.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "animation3d: an event needs a name.");
    return;
  }
  Event event;
  event.time = std::max(0.0f, time);
  event.name = std::move(name);

  const auto position = std::lower_bound(
      events_.begin(), events_.end(), event.time,
      [](const Event& left, float value) { return left.time < value; });
  events_.insert(position, std::move(event));
}

std::string Animation3D::EventName(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(events_.size()))
    return {};
  return events_[index].name;
}

void Animation3D::CollectEvents(float from,
                                float to,
                                std::vector<std::string>& out) const {
  const bool wrapped = to < from;
  for (const Event& event : events_) {
    const bool crossed = wrapped ? (event.time > from || event.time <= to)
                                 : (event.time > from && event.time <= to);
    if (crossed)
      out.push_back(event.name);
  }
}

float Animation3D::EventTime(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(events_.size()))
    return 0.0f;
  return events_[index].time;
}

void Animation3D::Apply(RefPtr<Skeleton3D> skeleton, float time) {
  if (skeleton->pose_open()) {
    Sample(skeleton.get(), time, 1.0f, nullptr);
    return;
  }

  skeleton->BeginPose();
  Sample(skeleton.get(), time, 1.0f, nullptr);
  skeleton->EndPose();
}

void Animation3D::Blend(URGE_EXCEPT,
                        RefPtr<Skeleton3D> skeleton,
                        float time,
                        float weight) {
  if (!skeleton->pose_open()) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "animation3d: call skeleton.begin_pose before blend.");
    return;
  }
  Sample(skeleton.get(), time, std::clamp(weight, 0.0f, 1.0f), nullptr);
}

void Animation3D::BlendMasked(URGE_EXCEPT,
                              RefPtr<Skeleton3D> skeleton,
                              float time,
                              float weight,
                              RefPtr<BoneMask3D> mask) {
  if (!skeleton->pose_open()) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "animation3d: call skeleton.begin_pose before blend_masked.");
    return;
  }

  Sample(skeleton.get(), time, std::clamp(weight, 0.0f, 1.0f),
         mask ? mask->weights() : nullptr);
}

void Animation3D::Sample(Skeleton3D* skeleton,
                         float time,
                         float weight,
                         const std::vector<float>* mask) {
  if (weight <= 0.0f)
    return;

  float local = time;
  if (duration_ > 0.0f && loop_) {
    local = std::fmod(local, duration_);
    if (local < 0.0f)
      local += duration_;
  }

  size_t index = 0;
  while (index < tracks_.size()) {
    const int32_t bone_index = tracks_[index].bone;
    RefPtr<Node3D> bone = skeleton->BoneAt(bone_index);
    if (!bone) {
      while (index < tracks_.size() && tracks_[index].bone == bone_index)
        ++index;
      continue;
    }

    const glm::vec4 base_rotation = bone->local_rotation();
    glm::vec3 position = bone->local_position();
    glm::vec3 scale = bone->local_scale();
    glm::quat rotation(base_rotation.w, base_rotation.x, base_rotation.y,
                       base_rotation.z);

    for (; index < tracks_.size() && tracks_[index].bone == bone_index;
         ++index) {
      const Track& track = tracks_[index];

      const size_t key = FindKey(track.times, local);
      const float start = track.times[key];

      float alpha = 0.0f;
      const bool has_next = key + 1 < track.times.size();
      if (has_next && track.interpolation != kStep) {
        const float span = track.times[key + 1] - start;
        if (span > 0.0f)
          alpha = std::clamp((local - start) / span, 0.0f, 1.0f);
      }

      const float* first = &track.values[key * track.stride];
      const float* second =
          has_next ? &track.values[(key + 1) * track.stride] : first;

      if (track.path == kRotation) {
        glm::quat left(first[3], first[0], first[1], first[2]);
        glm::quat right(second[3], second[0], second[1], second[2]);
        if (glm::dot(left, right) < 0.0f)
          right = -right;

        glm::quat mixed = glm::mix(left, right, alpha);
        if (glm::length(mixed) <= 0.0f)
          mixed = left;
        else
          mixed = glm::normalize(mixed);
        rotation = mixed;
      } else {
        const glm::vec3 left(first[0], first[1], first[2]);
        const glm::vec3 right(second[0], second[1], second[2]);
        const glm::vec3 mixed = glm::mix(left, right, alpha);
        if (track.path == kTranslation)
          position = mixed;
        else
          scale = mixed;
      }
    }

    skeleton->BlendBone(bone_index, position, rotation, scale, weight, mask);
  }
}

}
