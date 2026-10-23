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
#include <string>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "webgpu/webgpu_cpp.hpp"

#include "core/common/object.h"
#include "core/input/input.h"

namespace urge {

class ImGuiManager : public Singleton<ImGuiManager> {
 public:
  enum class FpsMode { kHidden = 0, kCompact = 1, kDetailed = 2 };

  explicit ImGuiManager(SDL_Window* window);
  ~ImGuiManager();

  ImGuiManager(const ImGuiManager&) = delete;
  ImGuiManager& operator=(const ImGuiManager&) = delete;

  void ProcessEvent(const SDL_Event& event);
  bool WantsGameInputBlocked() const { return settings_visible_; }
  void SetRenderTargetFormat(wgpu::TextureFormat format);
  void Update();
  void RenderDrawData(wgpu::RenderPassEncoder pass);

  bool settings_visible() const { return settings_visible_; }
  bool fps_visible() const { return fps_mode_ != FpsMode::kHidden; }
  FpsMode fps_mode() const { return fps_mode_; }

 private:
  float U(float value) const { return value * ui_scale_; }

  void BuildSettingsWindow();
  void BuildKeyboardPanel();
  void BuildGraphicsPanel();
  void BuildAudioPanel();
  void BuildAboutPanel();
  void BuildFPSWindow();
  void LoadFpsGeometry();
  void SaveFpsGeometry();
  void CycleFPSMode();

  void CaptureBinding(const Input::KeySym& binding);

  void StoreDisplaySettings();
  void SaveSettings();
  void ApplyUIScale();

  SDL_Window* window_ = nullptr;
  bool platform_ready_ = false;
  bool renderer_ready_ = false;
  wgpu::TextureFormat render_format_ = wgpu::TextureFormat::Undefined;
  bool settings_visible_ = false;
  FpsMode fps_mode_ = FpsMode::kHidden;
  int32_t capture_slot_ = -1;
  std::vector<Input::KeySym> default_bindings_;
  bool defaults_captured_ = false;
  std::vector<std::string> output_devices_;
  bool output_devices_stale_ = true;
  std::vector<float> frame_times_;
  float ui_scale_ = 1.0f;

  /* The frame monitor is the player's window, not the engine's: it is moved and
     resized like any other, and the geometry is kept in logical units so a
     change of display scale does not move it.  It is remembered across runs
     because a game that clips the overlay is a game whose overlay is gone for
     good otherwise.  The two modes are remembered apart -- a window made wide
     enough for the breakdown must not make the one-line preview tall. */
  struct FpsWindowGeometry {
    float width = 0.0f;
    float height = 0.0f;
    bool custom = false;
  };

  bool fps_geometry_loaded_ = false;
  bool fps_window_has_position_ = false;
  bool fps_geometry_dirty_ = false;
  FpsMode fps_applied_mode_ = FpsMode::kHidden;
  float fps_window_x_ = 0.0f;
  float fps_window_y_ = 0.0f;
  FpsWindowGeometry fps_window_[2];
};

}
