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

#include "core/imgui_manager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_misc.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

#include "core/audio.h"
#include "core/config.h"
#include "core/device.h"
#include "core/graphics.h"
#include "core/input.h"
#include "core/logger.h"

namespace urge {

namespace {

constexpr SDL_Scancode kToggleSettingsKey = SDL_SCANCODE_F1;
constexpr SDL_Scancode kToggleFPSKey = SDL_SCANCODE_F2;

constexpr const char* kKeySlots[] = {
    "DOWN", "LEFT", "RIGHT", "UP", "A", "B", "C", "X", "Y", "Z", "L", "R",
};
constexpr int32_t kKeySlotCount = static_cast<int32_t>(std::size(kKeySlots));

constexpr size_t kFrameTimeHistory = 240;

constexpr const char* kDefaultOutputLabel = "System default";

const char* const kFontCandidates[] = {
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/tahoma.ttf",
};

const char kLicenseSource[] = R"(MIT License

Copyright (c) 2026 Admenri Adev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.)";

const std::string& LicenseText() {
  static const std::string text = [] {
    std::string result = kLicenseSource;
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    return result;
  }();
  return text;
}

ImGuiStyle& BaseStyle() {
  static ImGuiStyle base = ImGui::GetStyle();
  return base;
}

}  // namespace

ImGuiManager::ImGuiManager(SDL_Window* window) : window_(window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigWindowsMoveFromTitleBarOnly = true;

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
  style.WindowBorderSize = 1.0f;

  bool font_loaded = false;
  for (const char* path : kFontCandidates) {
    std::error_code error;
    if (!std::filesystem::exists(path, error))
      continue;

    ImFontConfig config;
    config.SizePixels = 16.0f;
    if (io.Fonts->AddFontFromFileTTF(path, 16.0f, &config)) {
      font_loaded = true;
      LOGGER_INFO("debug UI font: {}", path);
      break;
    }
  }

  if (!font_loaded) {
    io.Fonts->AddFontDefault();
    LOGGER_WARN("no UI font was found; the debug UI uses the embedded face");
  }

  if (ImGui_ImplSDL3_InitForOther(window_))
    platform_ready_ = true;
  else
    LOGGER_ERROR("the SDL3 platform backend of the debug UI did not start");
}

ImGuiManager::~ImGuiManager() {
  if (!ImGui::GetCurrentContext())
    return;

  if (renderer_ready_) {
    ImGui_ImplWGPU_Shutdown();
    renderer_ready_ = false;
  }

  if (platform_ready_) {
    ImGui_ImplSDL3_Shutdown();
    platform_ready_ = false;
  }

  ImGui::DestroyContext();
}

void ImGuiManager::ProcessEvent(const SDL_Event& event) {
  if (!platform_ready_)
    return;

  ImGui_ImplSDL3_ProcessEvent(&event);

  if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
    return;

  const SDL_Scancode scancode = event.key.scancode;

  if (capture_slot_ >= 0) {
    const Input::KeySym captured{kKeySlots[capture_slot_],
                                 static_cast<int32_t>(scancode)};
    auto bindings = Input::Get().GetKeyBindings();
    if (std::find(bindings.begin(), bindings.end(), captured) == bindings.end())
      bindings.push_back(captured);
    Input::Get().SetKeyBindings(bindings);
    LOGGER_INFO("key binding {} += {}", kKeySlots[capture_slot_],
                SDL_GetScancodeName(scancode));
    capture_slot_ = -1;
    SaveSettings();
    return;
  }

  if (scancode == kToggleSettingsKey) {
    settings_visible_ = !settings_visible_;
    if (!settings_visible_) {
      capture_slot_ = -1;

      SaveSettings();
    }
    LOGGER_INFO("settings overlay {}", settings_visible_ ? "shown" : "hidden");
  } else if (scancode == kToggleFPSKey) {
    fps_visible_ = !fps_visible_;
  }
}

void ImGuiManager::SetRenderTargetFormat(wgpu::TextureFormat format) {
  if (!platform_ready_ || format == wgpu::TextureFormat::Undefined)
    return;

  if (renderer_ready_ && format == render_format_)
    return;

  if (renderer_ready_) {
    ImGui_ImplWGPU_Shutdown();
    renderer_ready_ = false;
  }

  ImGui_ImplWGPU_InitInfo info;
  info.Device = GPUDevice::Get().device().Get();
  info.NumFramesInFlight = 3;
  info.RenderTargetFormat = static_cast<WGPUTextureFormat>(format);
  info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  info.PipelineMultisampleState.count = 1;

  if (!ImGui_ImplWGPU_Init(&info)) {
    LOGGER_ERROR("the wgpu renderer backend of the debug UI did not start");
    return;
  }

  render_format_ = format;
  renderer_ready_ = true;
}

void ImGuiManager::Update() {
  if (!renderer_ready_)
    return;

  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplWGPU_NewFrame();
  ApplyUIScale();
  ImGui::NewFrame();

  const float delta_ms = ImGui::GetIO().DeltaTime * 1000.0f;
  frame_times_.push_back(delta_ms);
  if (frame_times_.size() > kFrameTimeHistory)
    frame_times_.erase(frame_times_.begin());

  if (fps_visible_)
    BuildFPSWindow();

  if (settings_visible_)
    BuildSettingsWindow();

  ImGui::Render();
}

void ImGuiManager::RenderDrawData(wgpu::RenderPassEncoder pass) {
  if (!renderer_ready_)
    return;

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (!draw_data || draw_data->CmdListsCount == 0)
    return;

  ImGui_ImplWGPU_RenderDrawData(draw_data, pass.Get());
}

void ImGuiManager::BuildSettingsWindow() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(14.0f), U(12.0f)));

  const bool open = ImGui::Begin("##urge_settings", nullptr, flags);
  ImGui::PopStyleVar(3);
  if (!open) {
    ImGui::End();
    return;
  }

  if (ImGui::BeginTabBar("##settings_tabs")) {
    if (ImGui::BeginTabItem("Key bindings")) {
      BuildKeyboardPanel();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Graphics")) {
      BuildGraphicsPanel();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Audio")) {
      BuildAudioPanel();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("About")) {
      BuildAboutPanel();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::End();
}

void ImGuiManager::BuildKeyboardPanel() {
  auto& input = Input::Get();

  std::vector<Input::KeySym> bindings = input.GetKeyBindings();
  if (!defaults_captured_) {
    default_bindings_ = bindings;
    defaults_captured_ = true;
  }

  bool changed = false;

  auto unbind = [&bindings](const char* slot, int32_t key) {
    const size_t before = bindings.size();
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [slot, key](const Input::KeySym& entry) {
                                    return entry.first == slot &&
                                           (key < 0 || entry.second == key);
                                  }),
                   bindings.end());
    return bindings.size() != before;
  };

  auto restore = [this, &bindings](const char* slot) {
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [slot](const Input::KeySym& entry) {
                                    return slot == nullptr ||
                                           entry.first == slot;
                                  }),
                   bindings.end());
    for (const auto& entry : default_bindings_)
      if (slot == nullptr || entry.first == slot)
        bindings.push_back(entry);
  };

  const ImGuiTableFlags table_flags =
      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_SizingStretchProp;

  const ImGuiStyle& style = ImGui::GetStyle();

  float slot_width = 0.0f;
  for (const char* slot : kKeySlots)
    slot_width = std::max(slot_width, ImGui::CalcTextSize(slot).x);
  slot_width += style.CellPadding.x * 4.0f;

  const float chip_suffix =
      ImGui::CalcTextSize(" x").x + style.FramePadding.x * 2.0f;
  float current_width = ImGui::CalcTextSize("(unbound)").x;
  for (int32_t s = 0; s < kKeySlotCount; ++s) {
    float width = 0.0f;
    int32_t count = 0;
    for (const auto& binding : bindings) {
      if (binding.first != kKeySlots[s])
        continue;
      if (count++)
        width += style.ItemSpacing.x;
      width += ImGui::CalcTextSize(input.GetKeyName(binding.second).c_str()).x +
               chip_suffix;
    }
    current_width = std::max(current_width, width);
  }

  if (ImGui::BeginTable("##key_bindings", 3, table_flags)) {
    ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, slot_width);
    ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed,
                            current_width + style.CellPadding.x * 2.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int32_t slot = 0; slot < kKeySlotCount; ++slot) {
      ImGui::TableNextRow();
      ImGui::PushID(slot);

      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(kKeySlots[slot]);

      ImGui::TableSetColumnIndex(1);
      int32_t dropped = -1;
      int32_t chip = 0;
      if (capture_slot_ != slot) {
        for (const auto& binding : bindings) {
          if (binding.first != kKeySlots[slot])
            continue;

          if (chip++)
            ImGui::SameLine();
          ImGui::PushID(binding.second);
          const std::string label = input.GetKeyName(binding.second) + " x";
          if (ImGui::SmallButton(label.c_str()))
            dropped = binding.second;
          ImGui::PopID();
        }

        if (chip == 0) {
          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted("(unbound)");
        }
      } else {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, 1.0f), "press a key...");
      }

      if (dropped >= 0) {
        if (unbind(kKeySlots[slot], dropped)) {
          LOGGER_INFO("key binding {} -= {}", kKeySlots[slot],
                      input.GetKeyName(dropped));
          changed = true;
        }
      }

      ImGui::TableSetColumnIndex(2);
      if (capture_slot_ == slot) {
        if (ImGui::Button("Cancel"))
          capture_slot_ = -1;
      } else {
        if (ImGui::Button("Add"))
          capture_slot_ = slot;
        ImGui::SameLine();
        if (ImGui::Button("Default")) {
          restore(kKeySlots[slot]);
          changed = true;
        }
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  ImGui::Spacing();
  if (ImGui::Button("Restore defaults")) {
    restore(nullptr);
    changed = true;
  }

  if (changed) {
    input.SetKeyBindings(bindings);
    SaveSettings();
  }
}

void ImGuiManager::BuildGraphicsPanel() {
  auto& graphics = Graphics::Get();

  bool changed = false;

  bool fullscreen = graphics.Attr_Fullscreen().value_or(false);
  if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
    graphics.Attr_Fullscreen(fullscreen);
    changed = true;
  }

  bool vsync = graphics.Attr_VSync().value_or(true);
  if (ImGui::Checkbox("VSync", &vsync)) {
    graphics.Attr_VSync(vsync);
    changed = true;
  }

  bool frame_skip = graphics.Attr_FrameSkip().value_or(true);
  if (ImGui::Checkbox("Frame skip", &frame_skip)) {
    graphics.Attr_FrameSkip(frame_skip);
    changed = true;
  }

  if (changed) {
    StoreDisplaySettings();
    SaveSettings();
  }
}

void ImGuiManager::BuildAudioPanel() {
  auto& audio = Audio::Get();

  auto save_on_release = [this] {
    if (ImGui::IsItemDeactivatedAfterEdit())
      SaveSettings();
  };

  int32_t master = audio.Attr_MasterVolume().value_or(100);
  if (ImGui::SliderInt("Master volume", &master, 0, 100))
    audio.Attr_MasterVolume(master);
  save_on_release();

  int32_t bgm = audio.Attr_BGMVolume().value_or(100);
  if (ImGui::SliderInt("BGM volume", &bgm, 0, 100))
    audio.Attr_BGMVolume(bgm);
  save_on_release();

  int32_t bgs = audio.Attr_BGSVolume().value_or(100);
  if (ImGui::SliderInt("BGS volume", &bgs, 0, 100))
    audio.Attr_BGSVolume(bgs);
  save_on_release();

  int32_t me = audio.Attr_MEVolume().value_or(100);
  if (ImGui::SliderInt("ME volume", &me, 0, 100))
    audio.Attr_MEVolume(me);
  save_on_release();

  int32_t se = audio.Attr_SEVolume().value_or(100);
  if (ImGui::SliderInt("SE volume", &se, 0, 100))
    audio.Attr_SEVolume(se);
  save_on_release();

  ImGui::Spacing();

  const std::string& current = audio.output_device();
  const char* preview = current.empty() ? kDefaultOutputLabel : current.c_str();

  if (ImGui::BeginCombo("Output device", preview)) {
    if (output_devices_stale_) {
      output_devices_ = audio.OutputDevices();
      output_devices_stale_ = false;
    }

    if (ImGui::Selectable(kDefaultOutputLabel, current.empty())) {
      if (audio.SetOutputDevice({}))
        SaveSettings();
    }

    for (const auto& device : output_devices_) {
      if (ImGui::Selectable(device.c_str(), device == current)) {
        if (audio.SetOutputDevice(device))
          SaveSettings();
      }
    }

    ImGui::EndCombo();
  } else {
    output_devices_stale_ = true;
  }
}

void ImGuiManager::BuildAboutPanel() {
  ImGui::TextUnformatted("URGE Engine");
  ImGui::Spacing();

  if (ImGui::Button("Open GitHub repository"))
    SDL_OpenURL("https://github.com/Admenri/urge");

  ImGui::Spacing();
  if (ImGui::BeginChild("##license", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders)) {
    ImGui::TextUnformatted(LicenseText().c_str());
  }
  ImGui::EndChild();
}

void ImGuiManager::BuildFPSWindow() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 size(U(270.0f), U(275.0f));

  const bool replace = fps_size_stale_ || fps_replace_position_;
  ImGui::SetNextWindowPos(
      ImVec2(viewport->Pos.x + viewport->Size.x - size.x - U(12.0f),
             viewport->Pos.y + U(12.0f)),
      replace ? ImGuiCond_Always : ImGuiCond_Once);
  ImGui::SetNextWindowSize(size,
                           fps_size_stale_ ? ImGuiCond_Always : ImGuiCond_Once);
  fps_size_stale_ = false;

  if (!ImGui::Begin("FPS monitor", &fps_visible_)) {
    ImGui::End();
    return;
  }

  const ImVec2 origin = ImGui::GetWindowPos();
  const ImVec2 extent = ImGui::GetWindowSize();
  fps_replace_position_ =
      origin.x + extent.x > viewport->Pos.x + viewport->Size.x ||
      origin.y + extent.y > viewport->Pos.y + viewport->Size.y;

  const ImGuiIO& io = ImGui::GetIO();
  const float frame_ms = frame_times_.empty() ? 0.0f : frame_times_.back();

  ImGui::Text("FPS");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.42f, 0.85f, 0.45f, 1.0f), "%.1f", io.Framerate);
  ImGui::SameLine();
  ImGui::TextDisabled("| %.2f ms", frame_ms);

  float minimum = 0.0f, maximum = 0.0f, total = 0.0f;
  if (!frame_times_.empty()) {
    minimum = maximum = frame_times_.front();
    for (float value : frame_times_) {
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
      total += value;
    }
  }
  const float average =
      frame_times_.empty() ? 0.0f : total / frame_times_.size();

  ImGui::Text("min / max  %.2f / %.2f ms", minimum, maximum);
  ImGui::Text("average    %.2f ms", average);

  const Graphics& graphics = Graphics::Get();
  ImGui::Text("render cost: %.2f ms", graphics.render_cost_ms());
  ImGui::Text("logic cost:  %.2f ms", graphics.logic_cost_ms());
  ImGui::Text("other cost:  %.2f ms", graphics.other_cost_ms());

  ImGui::Spacing();
  if (frame_times_.size() >= 2) {
    const float scale_max = std::max(1000.0f / 30.0f, maximum * 1.15f);
    ImGui::PlotLines("##frame_time", frame_times_.data(),
                     static_cast<int>(frame_times_.size()), 0, "ms", 0.0f,
                     scale_max, ImVec2(-FLT_MIN, U(84.0f)));
  } else {
    ImGui::TextDisabled("collecting...");
  }

  ImGui::End();
}

void ImGuiManager::StoreDisplaySettings() {
  auto& graphics = Graphics::Get();
  auto& settings = Config::Get().display;

  settings.fullscreen = graphics.Attr_Fullscreen().value_or(false);
  settings.vsync = graphics.Attr_VSync().value_or(true);
  settings.frame_skip = graphics.Attr_FrameSkip().value_or(true);
}

void ImGuiManager::SaveSettings() {
  Config::Get().key_bindings = Input::Get().GetKeyBindings();
  Config::Get().Save();
}

void ImGuiManager::ApplyUIScale() {
  ImGuiIO& io = ImGui::GetIO();
  ImGuiStyle& style = ImGui::GetStyle();

  const float logical_width =
      static_cast<float>(std::max(1, Graphics::Get().Width()));
  const float logical_height =
      static_cast<float>(std::max(1, Graphics::Get().Height()));
  const float scale = std::max(
      1.0f, std::min(io.DisplaySize.x / logical_width,
                     io.DisplaySize.y / logical_height));

  if (std::abs(scale - ui_scale_) < 0.01f)
    return;

  ui_scale_ = scale;
  fps_size_stale_ = true;
  LOGGER_INFO("debug UI scale {:.2f}", scale);

  style = BaseStyle();
  style.ScaleAllSizes(scale);
  style.FontScaleMain = scale;
}

}  // namespace urge
