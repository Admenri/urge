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

#include "core/scene3d/animation_state_machine3d.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/common/exception.h"
#include "core/scene3d/skeleton3d.h"

namespace urge {

AnimationStateMachine3D::AnimationStateMachine3D() = default;

AnimationStateMachine3D::~AnimationStateMachine3D() = default;

void AnimationStateMachine3D::AddState(URGE_EXCEPT,
                                       std::string name,
                                       RefPtr<Animation3D> animation,
                                       bool loop) {
  if (name.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "state machine 3d: a state needs a "
                          "name.");
    return;
  }
  if (FindState(name) >= 0) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "state machine 3d: the state '{}' already exists.",
                          name);
    return;
  }
  State state;
  state.name = std::move(name);
  state.animation = std::move(animation);
  state.loop = loop;
  states_.push_back(std::move(state));
}

void AnimationStateMachine3D::AddTransition(URGE_EXCEPT,
                                            std::string from,
                                            std::string to,
                                            float duration,
                                            float exit_time) {
  if (FindState(from) < 0 || FindState(to) < 0) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "state machine 3d: transition '{}' -> '{}' uses an unknown "
        "state.",
        from, to);
    return;
  }
  Transition transition;
  transition.from = std::move(from);
  transition.to = std::move(to);
  transition.duration = std::max(0.0f, duration);
  transition.exit_time = exit_time;
  transitions_.push_back(std::move(transition));
}

int32_t AnimationStateMachine3D::FindState(const std::string& name) const {
  for (size_t i = 0; i < states_.size(); ++i)
    if (states_[i].name == name)
      return static_cast<int32_t>(i);
  return -1;
}

const AnimationStateMachine3D::Transition*
AnimationStateMachine3D::FindTransition(const std::string& from,
                                        const std::string& to) const {
  for (const Transition& transition : transitions_)
    if (transition.from == from && transition.to == to)
      return &transition;
  return nullptr;
}

void AnimationStateMachine3D::AdvanceTime(float& time,
                                          const State& state,
                                          float delta) const {
  time += delta;

  const float duration = state.animation->Duration();
  if (state.loop && duration > 0.0f) {
    time = std::fmod(time, duration);
    if (time < 0.0f)
      time += duration;
  }
}

bool AnimationStateMachine3D::Play(URGE_EXCEPT, std::string name) {
  const int32_t index = FindState(name);
  if (index < 0) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "state machine 3d: the state '{}' does not exist.",
                          name);
    return {};
  }
  if (index == current_index_)
    return false;

  if (current_index_ >= 0) {
    previous_index_ = current_index_;
    previous_time_ = current_time_;

    const Transition* transition =
        FindTransition(states_[current_index_].name, name);
    blend_duration_ = transition ? transition->duration : 0.2f;
    blend_time_ = 0.0f;
    blending_ = blend_duration_ > 0.0f;
  } else {
    previous_index_ = -1;
    blend_duration_ = 0.0f;
    blend_time_ = 0.0f;
    blending_ = false;
  }

  current_index_ = index;
  current_time_ = 0.0f;
  current_name_ = std::move(name);
  return true;
}

void AnimationStateMachine3D::Update(URGE_EXCEPT,
                                     RefPtr<Skeleton3D> skeleton,
                                     float delta) {
  if (current_index_ < 0)
    return;

  State& current = states_[current_index_];
  const float from = current_time_;
  AdvanceTime(current_time_, current, delta);
  current.animation->CollectEvents(from, current_time_, events_);

  if (blending_) {
    blend_time_ += delta;
    if (blend_time_ >= blend_duration_) {
      blending_ = false;
      previous_index_ = -1;
    }
  } else {
    for (const Transition& transition : transitions_) {
      if (transition.from != current.name || transition.exit_time < 0.0f)
        continue;
      if (current_time_ >= transition.exit_time) {
        Play(exception_state, transition.to);
        return;
      }
    }
  }

  skeleton->BeginPose();

  if (blending_ && previous_index_ >= 0) {
    State& previous = states_[previous_index_];
    AdvanceTime(previous_time_, previous, delta);

    const float weight = std::clamp(
        blend_time_ / std::max(blend_duration_, 0.0001f), 0.0f, 1.0f);
    previous.animation->Blend(exception_state, skeleton, previous_time_,
                              1.0f - weight);
    current.animation->Blend(exception_state, skeleton, current_time_, weight);
  } else {
    current.animation->Blend(exception_state, skeleton, current_time_, 1.0f);
  }

  skeleton->EndPose();
}

std::vector<std::string> AnimationStateMachine3D::TakeEvents() {
  std::vector<std::string> result;
  result.swap(events_);
  return result;
}

}
