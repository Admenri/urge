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

#include "core/input/touch.h"

#include <algorithm>
#include <utility>

#include "core/graphics/graphics.h"

namespace urge {

Touch::Touch() = default;

Touch::~Touch() = default;

void Touch::Update() {
  for (auto& finger : fingers_) {
    if (finger.id < 0)
      continue;

    const bool raw = finger.raw_down;

    finger.down = finger.tapped || (!finger.pressed && raw);
    finger.up = (finger.pressed && !raw) || (finger.tapped && !raw);
    finger.pressed = raw;

    finger.moved = finger.pressed &&
                   (finger.x != finger.last_x || finger.y != finger.last_y);

    finger.last_x = finger.x;
    finger.last_y = finger.y;
    finger.tapped = false;
  }
}

int32_t Touch::Count() {
  int32_t count = 0;
  for (const auto& finger : fingers_)
    if (finger.id >= 0 && finger.pressed)
      ++count;
  return count;
}

std::vector<int64_t> Touch::Ids() {
  std::vector<std::pair<uint64_t, int64_t>> ordered;
  for (const auto& finger : fingers_)
    if (finger.id >= 0 &&
        (finger.pressed || finger.down || finger.up || finger.moved))
      ordered.emplace_back(finger.order, finger.id);

  std::sort(ordered.begin(), ordered.end());

  std::vector<int64_t> ids;
  ids.reserve(ordered.size());
  for (const auto& entry : ordered)
    ids.push_back(entry.second);
  return ids;
}

int64_t Touch::IdAt(int32_t index) {
  if (index < 0)
    return -1;

  const std::vector<int64_t> ids = Ids();
  if (static_cast<size_t>(index) >= ids.size())
    return -1;
  return ids[static_cast<size_t>(index)];
}

int64_t Touch::FirstId() {
  int64_t result = -1;
  uint64_t earliest = 0;
  for (const auto& finger : fingers_) {
    if (finger.id < 0 || !finger.pressed)
      continue;
    if (result < 0 || finger.order < earliest) {
      result = finger.id;
      earliest = finger.order;
    }
  }
  return result;
}

bool Touch::IsDown(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->down : false;
}

bool Touch::IsPress(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->pressed : false;
}

bool Touch::IsUp(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->up : false;
}

bool Touch::IsMove(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->moved : false;
}

float Touch::X(int64_t id) {
  const Finger* finger = Find(id);
  if (!finger)
    return 0.0f;
  return Graphics::Get().NormalizedToScreen(finger->x, finger->y).x;
}

float Touch::Y(int64_t id) {
  const Finger* finger = Find(id);
  if (!finger)
    return 0.0f;
  return Graphics::Get().NormalizedToScreen(finger->x, finger->y).y;
}

float Touch::DeltaX(int64_t id) {
  const Finger* finger = Find(id);
  if (!finger)
    return 0.0f;
  return Graphics::Get()
      .NormalizedToScreenDelta(finger->x - finger->last_x,
                               finger->y - finger->last_y)
      .x;
}

float Touch::DeltaY(int64_t id) {
  const Finger* finger = Find(id);
  if (!finger)
    return 0.0f;
  return Graphics::Get()
      .NormalizedToScreenDelta(finger->x - finger->last_x,
                               finger->y - finger->last_y)
      .y;
}

float Touch::Pressure(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->pressure : 0.0f;
}

float Touch::RawX(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->x : 0.0f;
}

float Touch::RawY(int64_t id) {
  const Finger* finger = Find(id);
  return finger ? finger->y : 0.0f;
}

ATTR_DEF(Touch, bool, Enabled) {
  if (value.has_value()) {
    enabled_ = *value;

    if (!enabled_)
      CancelAll();

    return std::nullopt;
  }

  return enabled_;
}

void Touch::ProcessEvents(URGE_EXCEPT, SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_FINGER_DOWN: {
      if (!enabled_)
        break;

      const SDL_TouchFingerEvent& finger = event->tfinger;
      Finger* slot = Acquire(finger.fingerID);
      if (!slot)
        break;

      slot->id = finger.fingerID;
      slot->raw_down = true;
      slot->tapped = true;
      slot->pressed = false;
      slot->down = false;
      slot->up = false;
      slot->moved = false;
      slot->x = finger.x;
      slot->y = finger.y;
      slot->last_x = finger.x;
      slot->last_y = finger.y;
      slot->pressure = finger.pressure;
      slot->order = ++order_counter_;
      break;
    }

    case SDL_EVENT_FINGER_MOTION: {
      if (!enabled_)
        break;

      const SDL_TouchFingerEvent& finger = event->tfinger;
      Finger* slot = Find(finger.fingerID);
      if (!slot)
        break;

      slot->x = finger.x;
      slot->y = finger.y;
      slot->pressure = finger.pressure;
      break;
    }

    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
      Finger* slot = Find(event->tfinger.fingerID);
      if (!slot)
        break;

      slot->raw_down = false;
      slot->pressure = 0.0f;
      break;
    }

    default:
      break;
  }
}

Touch::Finger* Touch::Find(int64_t id) {
  for (auto& finger : fingers_)
    if (finger.id == id)
      return &finger;
  return nullptr;
}

const Touch::Finger* Touch::Find(int64_t id) const {
  for (const auto& finger : fingers_)
    if (finger.id == id)
      return &finger;
  return nullptr;
}

Touch::Finger* Touch::Acquire(int64_t id) {
  if (Finger* existing = Find(id))
    return existing;

  for (auto& finger : fingers_)
    if (finger.id < 0)
      return &finger;

  Finger* oldest = nullptr;
  for (auto& finger : fingers_)
    if (!finger.raw_down && (!oldest || finger.order < oldest->order))
      oldest = &finger;

  if (!oldest)
    return nullptr;

  *oldest = Finger{};
  return oldest;
}

void Touch::CancelAll() {
  for (auto& finger : fingers_)
    finger.raw_down = false;
}

}
