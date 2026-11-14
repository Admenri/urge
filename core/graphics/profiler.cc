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

#include "core/graphics/profiler.h"

#include <algorithm>

#include "SDL3/SDL_timer.h"

#include "core/render/render_stats.h"

namespace urge {

void Profiler::BeginRecord(Kind kind) {
  kind_ = kind;
  begin_tick_ = SDL_GetPerformanceCounter();
}

void Profiler::EndRecord() {
  cost_ms_[kind_] =
      TicksToMilliseconds(SDL_GetPerformanceCounter() - begin_tick_);
}

double Profiler::BeginFrame(std::uint64_t frame_begin) {
  const double script_span =
      last_update_end_tick_ == 0
          ? 0.0
          : TicksToMilliseconds(frame_begin - last_update_end_tick_);
  cost_ms_[kLogic] = script_span;
  return script_span;
}

double Profiler::EndFrame(std::uint64_t frame_begin) {
  last_update_end_tick_ = SDL_GetPerformanceCounter();
  const double update_span =
      TicksToMilliseconds(last_update_end_tick_ - frame_begin);
  cost_ms_[kOther] = std::max(0.0, update_span - render_cost_ms());
  return update_span;
}

void Profiler::ResumeFromBackground() {
  last_update_end_tick_ = SDL_GetPerformanceCounter();
}

}  // namespace urge