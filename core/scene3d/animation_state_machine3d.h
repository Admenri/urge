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

namespace urge {

class Skeleton3D;

URGE_BINDING()
class AnimationStateMachine3D : public Object {
 public:
  URGE_BINDING()
  AnimationStateMachine3D();
  ~AnimationStateMachine3D() override;

  URGE_BINDING()
  void AddState(URGE_EXCEPT,
                std::string name,
                RefPtr<Animation3D> animation,
                bool loop);
  URGE_BINDING()
  void AddTransition(URGE_EXCEPT,
                     std::string from,
                     std::string to,
                     float duration,
                     float exit_time);

  URGE_BINDING()
  bool Play(URGE_EXCEPT, std::string name);
  URGE_BINDING()
  void Update(URGE_EXCEPT, RefPtr<Skeleton3D> skeleton, float delta);

  URGE_BINDING(Name : "state")
  std::string CurrentState() const { return current_name_; }
  URGE_BINDING(Name : "blending")
  bool Blending() const { return blending_; }
  URGE_BINDING()
  float StateTime() const { return current_time_; }

  URGE_BINDING()
  std::vector<std::string> TakeEvents();

 private:
  struct State {
    std::string name;
    RefPtr<Animation3D> animation;
    bool loop = true;
  };

  struct Transition {
    std::string from;
    std::string to;
    float duration = 0.2f;
    float exit_time = -1.0f;
  };

  int32_t FindState(const std::string& name) const;
  const Transition* FindTransition(const std::string& from,
                                   const std::string& to) const;
  void AdvanceTime(float& time, const State& state, float delta) const;

  std::vector<State> states_;
  std::vector<Transition> transitions_;
  std::vector<std::string> events_;

  int32_t current_index_ = -1;
  int32_t previous_index_ = -1;
  std::string current_name_;
  float current_time_ = 0.0f;
  float previous_time_ = 0.0f;
  float blend_time_ = 0.0f;
  float blend_duration_ = 0.0f;
  bool blending_ = false;
};

}
