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

#include <array>
#include <cstdint>
#include <vector>

#include "SDL3/SDL_events.h"

#include "core/common/definition.h"
#include "core/common/object.h"

namespace urge {

URGE_BINDING()
class Touch : public Singleton<Touch> {
 public:
  static constexpr int32_t kMaxFingers = 16;

  Touch();
  ~Touch();

  URGE_BINDING()
  void Update();

  URGE_BINDING()
  int32_t Count();

  URGE_BINDING()
  std::vector<int64_t> Ids();

  URGE_BINDING(Name : "id_at")
  int64_t IdAt(int32_t index);

  URGE_BINDING(Name : "first_id")
  int64_t FirstId();

  URGE_BINDING(Name : "down?")
  bool IsDown(int64_t id);

  URGE_BINDING(Name : "press?")
  bool IsPress(int64_t id);

  URGE_BINDING(Name : "up?")
  bool IsUp(int64_t id);

  URGE_BINDING(Name : "move?")
  bool IsMove(int64_t id);

  URGE_BINDING()
  float X(int64_t id);
  URGE_BINDING()
  float Y(int64_t id);
  URGE_BINDING()
  float DeltaX(int64_t id);
  URGE_BINDING()
  float DeltaY(int64_t id);
  URGE_BINDING()
  float Pressure(int64_t id);

  URGE_BINDING(Name : "raw_x")
  float RawX(int64_t id);
  URGE_BINDING(Name : "raw_y")
  float RawY(int64_t id);

  URGE_BINDING()
  ATTR(bool, Enabled);

  void ProcessEvents(URGE_EXCEPT, SDL_Event* event);

  void CancelAll();

 private:
  struct Finger {
    int64_t id = -1;
    bool raw_down = false;
    bool tapped = false;
    bool pressed = false;
    bool down = false;
    bool up = false;
    bool moved = false;
    float x = 0.0f;
    float y = 0.0f;
    float last_x = 0.0f;
    float last_y = 0.0f;
    float pressure = 0.0f;
    uint64_t order = 0;
  };

  Finger* Find(int64_t id);
  const Finger* Find(int64_t id) const;

  Finger* Acquire(int64_t id);

  std::array<Finger, kMaxFingers> fingers_ = {};
  uint64_t order_counter_ = 0;
  bool enabled_ = true;
};

}
