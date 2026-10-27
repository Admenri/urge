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

#include "core/render/imgui_manager.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <string>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_misc.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

#include "core/audio/audio.h"
#include "core/common/logger.h"
#include "core/font/font_context.h"
#include "core/graphics/graphics.h"
#include "core/input/input.h"
#include "core/render/device.h"
#include "core/render/render_stats.h"
#include "core/system/config.h"

#include <cfloat>
#include <cstdio>

namespace urge {

namespace {

constexpr SDL_Scancode kToggleSettingsKey = SDL_SCANCODE_F1;
constexpr SDL_Scancode kToggleFPSKey = SDL_SCANCODE_F2;

constexpr const char* kKeySlots[] = {
    "DOWN", "LEFT", "RIGHT", "UP", "A", "B", "C", "X", "Y", "Z", "L", "R",
};
constexpr int32_t kKeySlotCount = static_cast<int32_t>(std::size(kKeySlots));

constexpr const char* kDefaultOutputLabel = "System default";

constexpr float kUiFontSize = 16.0f;

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

}

ImGuiManager::ImGuiManager(SDL_Window* window) : window_(window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigWindowsMoveFromTitleBarOnly = true;

  if (Config::Get().HasCommandLine("fpsmonitor")) {
    fps_mode_ = FpsMode::kDetailed;
    LOGGER_INFO("frame monitor detailed (command line)");
  }

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
  style.WindowBorderSize = 1.0f;

  bool font_loaded = false;
  const std::string& ui_font = FontContext::Get().default_font();
  if (const FontContext::FontData* data =
          FontContext::Get().FindFont(ui_font)) {
    ImFontConfig config;
    config.SizePixels = kUiFontSize;
    config.FontDataOwnedByAtlas = false;

    if (io.Fonts->AddFontFromMemoryTTF(data->data, static_cast<int>(data->size),
                                       kUiFontSize, &config)) {
      font_loaded = true;
      LOGGER_INFO("debug UI font: {} ({} KiB)", ui_font, data->size / 1024);
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

void ImGuiManager::CaptureBinding(const Input::KeySym& binding) {
  auto bindings = Input::Get().GetKeyBindings();
  if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end())
    bindings.push_back(binding);

  Input::Get().SetKeyBindings(bindings);
  LOGGER_INFO("key binding {} += {}", binding.name,
              Input::Get().BindingName(binding));
  capture_slot_ = -1;
  SaveSettings();
}

void ImGuiManager::ProcessEvent(const SDL_Event& event) {
  if (!platform_ready_)
    return;

  ImGui_ImplSDL3_ProcessEvent(&event);

  if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    if (capture_slot_ >= 0)
      CaptureBinding(Input::KeySym{kKeySlots[capture_slot_],
                                   static_cast<int32_t>(event.gbutton.button),
                                   true});
    return;
  }

  if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
    return;

  const SDL_Scancode scancode = event.key.scancode;

  if (capture_slot_ >= 0) {
    CaptureBinding(Input::KeySym{kKeySlots[capture_slot_],
                                 static_cast<int32_t>(scancode), false});
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
    CycleFPSMode();
  }
}

void ImGuiManager::CycleFPSMode() {
  fps_mode_ = static_cast<FpsMode>((static_cast<int>(fps_mode_) + 1) % 3);

  static const char* const kNames[] = {"hidden", "compact", "detailed"};
  LOGGER_INFO("frame monitor {}", kNames[static_cast<int>(fps_mode_)]);
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

  if (fps_visible())
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

  auto restore = [this, &bindings](const char* slot) {
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [slot](const Input::KeySym& entry) {
                                    return slot == nullptr ||
                                           entry.name == slot;
                                  }),
                   bindings.end());
    for (const auto& entry : default_bindings_)
      if (slot == nullptr || entry.name == slot)
        bindings.push_back(entry);
  };

  const ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerH |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp;

  const ImGuiStyle& style = ImGui::GetStyle();

  const GamepadManager& gamepads = input.gamepads();
  if (const GamepadDevice* pad = gamepads.primary()) {
    if (gamepads.count() > 1)
      ImGui::TextDisabled("Gamepad: %s (+%d)", pad->name().c_str(),
                          gamepads.count() - 1);
    else
      ImGui::TextDisabled("Gamepad: %s", pad->name().c_str());
  } else {
    ImGui::TextDisabled("Gamepad: not connected");
  }

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
      if (binding.name != kKeySlots[s])
        continue;
      if (count++)
        width += style.ItemSpacing.x;
      width += ImGui::CalcTextSize(input.BindingName(binding).c_str()).x +
               chip_suffix;
    }
    current_width = std::max(current_width, width);
  }

  if (ImGui::BeginTable("##key_bindings", 3, table_flags)) {
    ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed,
                            slot_width);
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
      size_t dropped = bindings.size();
      int32_t chip = 0;
      if (capture_slot_ != slot) {
        for (size_t i = 0; i < bindings.size(); ++i) {
          const Input::KeySym& binding = bindings[i];
          if (binding.name != kKeySlots[slot])
            continue;

          if (chip++)
            ImGui::SameLine();
          ImGui::PushID(binding.gamepad ? -binding.code - 1 : binding.code);
          const std::string label = input.BindingName(binding) + " x";
          if (ImGui::SmallButton(label.c_str()))
            dropped = i;
          ImGui::PopID();
        }

        if (chip == 0) {
          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted("(unbound)");
        }
      } else {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, 1.0f),
                           "press a key or a pad button...");
      }

      if (dropped < bindings.size()) {
        LOGGER_INFO("key binding {} -= {}", kKeySlots[slot],
                    input.BindingName(bindings[dropped]));
        bindings.erase(bindings.begin() + dropped);
        changed = true;
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

namespace {

struct PhaseStyle {
  FramePhase phase;
  const char* label;
  ImVec4 color;
};

const PhaseStyle kPhaseStyles[kFramePhaseCount] = {
    {FramePhase::kScript, "Scripts", ImVec4(0.96f, 0.66f, 0.26f, 1.0f)},
    {FramePhase::kUpdate, "Graphics::Update",
     ImVec4(0.36f, 0.63f, 0.96f, 1.0f)},
    {FramePhase::kBitmap, "BitmapQueue", ImVec4(0.30f, 0.82f, 0.72f, 1.0f)},
    {FramePhase::kPresent, "Present", ImVec4(0.71f, 0.50f, 0.96f, 1.0f)},
    {FramePhase::kOther, "Other", ImVec4(0.55f, 0.58f, 0.63f, 1.0f)},
};

constexpr float kTwoPi = 6.28318530718f;

const ImVec4 kGood(0.42f, 0.85f, 0.45f, 1.0f);
const ImVec4 kWarn(0.95f, 0.78f, 0.28f, 1.0f);
const ImVec4 kBad(0.94f, 0.42f, 0.40f, 1.0f);

void DrawPhaseDonut(const FrameTimings& timings, float radius) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 center(origin.x + radius, origin.y + radius);

  int visible = 0;
  for (const PhaseStyle& style : kPhaseStyles) {
    if (timings.phase(style.phase) > 0.0)
      ++visible;
  }

  const double total = timings.total_ms;
  if (total > 0.0 && visible > 0) {
    const float gap = visible > 1 ? 0.02f : 0.0f;
    float angle = -kTwoPi * 0.25f;

    ImVector<ImVec2> points;
    for (const PhaseStyle& style : kPhaseStyles) {
      const double value = timings.phase(style.phase);
      if (value <= 0.0)
        continue;

      const float sweep = static_cast<float>(value / total) * kTwoPi;
      const float begin = angle + gap * 0.5f;
      const float end = angle + sweep - gap * 0.5f;
      angle += sweep;
      if (end <= begin)
        continue;

      const int segments = std::max(
          2, static_cast<int>(std::ceil((end - begin) / kTwoPi * 120.0f)));

      points.clear();
      points.push_back(center);
      for (int i = 0; i <= segments; ++i) {
        const float a =
            begin + (end - begin) * static_cast<float>(i) / segments;
        points.push_back(ImVec2(center.x + std::cos(a) * radius,
                                center.y + std::sin(a) * radius));
      }

      draw->AddConvexPolyFilled(points.Data, points.Size,
                                ImGui::GetColorU32(style.color));
    }
  } else {
    draw->AddCircleFilled(center, radius, ImGui::GetColorU32(ImGuiCol_FrameBg),
                          64);
  }

  draw->AddCircleFilled(center, radius * 0.62f,
                        ImGui::GetColorU32(ImGuiCol_WindowBg), 64);

  char value[32];
  snprintf(value, sizeof(value), "%.1f", total);
  const ImVec2 value_size = ImGui::CalcTextSize(value);
  const ImVec2 unit_size = ImGui::CalcTextSize("ms");
  draw->AddText(
      ImVec2(center.x - value_size.x * 0.5f, center.y - value_size.y * 0.92f),
      ImGui::GetColorU32(ImGuiCol_Text), value);
  draw->AddText(
      ImVec2(center.x - unit_size.x * 0.5f, center.y + value_size.y * 0.08f),
      ImGui::GetColorU32(ImGuiCol_TextDisabled), "ms");

  ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

void DrawPhaseLegend(const FrameTimings& timings, float width) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImGuiStyle& style = ImGui::GetStyle();

  const double total = timings.total_ms;
  const float line = ImGui::GetTextLineHeight();
  const float row_height = line + style.ItemSpacing.y * 0.5f;
  const float box = line * 0.7f;

  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float right = start.x + width;
  float y = start.y;

  for (const PhaseStyle& style_entry : kPhaseStyles) {
    const double value = timings.phase(style_entry.phase);
    const double share = total > 0.0 ? value / total * 100.0 : 0.0;

    char text[64];
    snprintf(text, sizeof(text), "%.2f ms   %.1f%%", value, share);
    const ImVec2 text_size = ImGui::CalcTextSize(text);

    const float value_x = right - text_size.x;
    const float label_x = start.x + box + style.ItemSpacing.x;

    draw->AddRectFilled(ImVec2(start.x, y + (line - box) * 0.5f),
                        ImVec2(start.x + box, y + (line + box) * 0.5f),
                        ImGui::GetColorU32(style_entry.color), 2.0f);

    draw->PushClipRect(
        ImVec2(label_x, y),
        ImVec2(std::max(label_x, value_x - style.ItemSpacing.x), y + line),
        true);
    draw->AddText(ImVec2(label_x, y), ImGui::GetColorU32(ImGuiCol_Text),
                  style_entry.label);
    draw->PopClipRect();

    draw->AddText(ImVec2(value_x, y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  text);

    y += row_height;
  }

  ImGui::Dummy(ImVec2(width, y - start.y));
}

void DrawPhaseHistory(const FrameTimingRing& ring,
                      float width,
                      float height,
                      double budget_ms) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 far_corner(origin.x + width, origin.y + height);

  draw->AddRectFilled(origin, far_corner, ImGui::GetColorU32(ImGuiCol_FrameBg),
                      3.0f);

  const std::size_t count = ring.size();
  if (count == 0) {
    ImGui::Dummy(ImVec2(width, height));
    return;
  }

  double peak = std::max(budget_ms, 1.0);
  for (std::size_t i = 0; i < count; ++i)
    peak = std::max(peak, ring.At(i).total_ms);
  peak *= 1.1;

  const float slot = width / static_cast<float>(FrameTimingRing::kCapacity);
  const float bar = std::max(1.0f, slot - 1.0f);

  draw->PushClipRect(origin, far_corner, true);
  for (std::size_t i = 0; i < count; ++i) {
    const FrameTimings& sample = ring.At(i);
    const float x = origin.x + width - static_cast<float>(count - i) * slot;

    float y = origin.y + height;
    for (const PhaseStyle& style : kPhaseStyles) {
      const double value = sample.phase(style.phase);
      if (value <= 0.0)
        continue;

      const float phase_height = static_cast<float>(value / peak) * height;
      draw->AddRectFilled(ImVec2(x, y - phase_height), ImVec2(x + bar, y),
                          ImGui::GetColorU32(style.color));
      y -= phase_height;
    }
  }

  if (budget_ms > 0.0) {
    const float y =
        origin.y + height - static_cast<float>(budget_ms / peak) * height;
    draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + width, y),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
  }
  draw->PopClipRect();

  ImGui::Dummy(ImVec2(width, height));
}

float ParseGeometry(const std::string& text, float fallback) {
  if (text.empty())
    return fallback;

  char* end = nullptr;
  const float value = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(value))
    return fallback;
  return value;
}

}

void ImGuiManager::LoadFpsGeometry() {
  fps_geometry_loaded_ = true;

  auto& config = Config::Get();

  static constexpr const char* kWidthKeys[2] = {"FpsWindowCompactWidth",
                                                "FpsWindowDetailedWidth"};
  static constexpr const char* kHeightKeys[2] = {"FpsWindowCompactHeight",
                                                 "FpsWindowDetailedHeight"};

  for (int mode = 0; mode < 2; ++mode) {
    FpsWindowGeometry& geometry = fps_window_[mode];
    geometry.width =
        ParseGeometry(config.GetValue("Debug", kWidthKeys[mode]), 0.0f);
    geometry.height =
        ParseGeometry(config.GetValue("Debug", kHeightKeys[mode]), 0.0f);
    geometry.custom = geometry.width > 0.0f && geometry.height > 0.0f;
  }

  const float x = ParseGeometry(config.GetValue("Debug", "FpsWindowX"), -1.0f);
  const float y = ParseGeometry(config.GetValue("Debug", "FpsWindowY"), -1.0f);

  fps_window_has_position_ = x >= 0.0f && y >= 0.0f;
  fps_window_x_ = x;
  fps_window_y_ = y;
}

void ImGuiManager::SaveFpsGeometry() {
  auto& config = Config::Get();

  static constexpr const char* kWidthKeys[2] = {"FpsWindowCompactWidth",
                                                "FpsWindowDetailedWidth"};
  static constexpr const char* kHeightKeys[2] = {"FpsWindowCompactHeight",
                                                 "FpsWindowDetailedHeight"};

  config.SetValue("Debug", "FpsWindowX",
                  std::to_string(static_cast<int>(fps_window_x_)));
  config.SetValue("Debug", "FpsWindowY",
                  std::to_string(static_cast<int>(fps_window_y_)));

  for (int mode = 0; mode < 2; ++mode) {
    const FpsWindowGeometry& geometry = fps_window_[mode];
    if (!geometry.custom)
      continue;
    config.SetValue("Debug", kWidthKeys[mode],
                    std::to_string(static_cast<int>(geometry.width)));
    config.SetValue("Debug", kHeightKeys[mode],
                    std::to_string(static_cast<int>(geometry.height)));
  }

  ExceptionState save_state;
  config.Save(save_state);
  if (save_state.HadException())
    LOGGER_ERROR("[ImGui] {}", save_state.message());
}

void ImGuiManager::BuildFPSWindow() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const bool detailed = fps_mode_ == FpsMode::kDetailed;
  const int mode = detailed ? 1 : 0;

  if (!fps_geometry_loaded_)
    LoadFpsGeometry();

  const float margin = U(12.0f);
  const ImVec2 display = viewport->Size;

  const bool mode_changed = fps_applied_mode_ != fps_mode_;
  fps_applied_mode_ = fps_mode_;

  const ImVec2 default_size(detailed ? U(340.0f) : U(232.0f),
                            detailed ? U(520.0f) : U(124.0f));

  FpsWindowGeometry& geometry = fps_window_[mode];

  ImVec2 size(geometry.width * ui_scale_, geometry.height * ui_scale_);
  if (!geometry.custom || size.x < 1.0f || size.y < 1.0f)
    size = default_size;

  ImVec2 pos(fps_window_x_ * ui_scale_, fps_window_y_ * ui_scale_);
  if (!fps_window_has_position_)
    pos = ImVec2(viewport->Pos.x + display.x - size.x - margin,
                 viewport->Pos.y + margin);

  ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(size, mode_changed && !geometry.custom
                                     ? ImGuiCond_Always
                                     : ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(U(148.0f), U(44.0f)),
                                      ImVec2(display.x, display.y));

  if (!ImGui::Begin("Frame monitor", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  {
    const ImVec2 actual_pos = ImGui::GetWindowPos();
    const ImVec2 actual_size = ImGui::GetWindowSize();

    if (std::abs(actual_size.x - size.x) > 1.0f ||
        std::abs(actual_size.y - size.y) > 1.0f)
      geometry.custom = true;

    const float width = actual_size.x / ui_scale_;
    const float height = actual_size.y / ui_scale_;
    if (std::abs(actual_pos.x / ui_scale_ - fps_window_x_) > 0.5f ||
        std::abs(actual_pos.y / ui_scale_ - fps_window_y_) > 0.5f ||
        std::abs(width - geometry.width) > 0.5f ||
        std::abs(height - geometry.height) > 0.5f) {
      fps_window_x_ = actual_pos.x / ui_scale_;
      fps_window_y_ = actual_pos.y / ui_scale_;
      geometry.width = width;
      geometry.height = height;
      fps_window_has_position_ = true;
      fps_geometry_dirty_ = true;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      const float right = viewport->Pos.x + display.x;
      const float bottom = viewport->Pos.y + display.y;

      ImVec2 fixed = actual_pos;
      if (fixed.x + actual_size.x > right)
        fixed.x = right - actual_size.x;
      if (fixed.y + actual_size.y > bottom)
        fixed.y = bottom - actual_size.y;

      fixed.x = std::min(std::max(fixed.x, viewport->Pos.x),
                         std::max(viewport->Pos.x, right - U(64.0f)));
      fixed.y = std::min(std::max(fixed.y, viewport->Pos.y),
                         std::max(viewport->Pos.y, bottom - U(28.0f)));

      if (fixed.x != actual_pos.x || fixed.y != actual_pos.y) {
        ImGui::SetWindowPos(fixed);
        fps_window_x_ = fixed.x / ui_scale_;
        fps_window_y_ = fixed.y / ui_scale_;
        fps_geometry_dirty_ = true;
      }
    }
  }

  if (fps_geometry_dirty_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    SaveFpsGeometry();
    fps_geometry_dirty_ = false;
  }

  const RenderStats& stats = GetRenderStats();
  const FrameTimeHistory& history = stats.frame_history;
  const FrameTimings& timings = stats.timings;

  const int32_t target_rate =
      std::max(1, Graphics::Get().Attr_FrameRate().value_or(60));
  const double budget_ms = 1000.0 / static_cast<double>(target_rate);

  const double recent_ms = history.RecentAverage(60);
  const double fps = recent_ms > 0.0 ? 1000.0 / recent_ms : 0.0;
  const double average_ms = history.Average();
  const double one_percent_ms = history.SlowAverage(0.01);

  const ImVec4 rate_color =
      fps >= static_cast<double>(target_rate) * 0.97
          ? kGood
          : (fps >= static_cast<double>(target_rate) * 0.80 ? kWarn : kBad);

  {
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextColored(rate_color, "%.1f fps", fps);
    ImGui::PopFont();
  }

  ImGui::TextDisabled("%.2f ms   avg %.1f   1%% low %.1f", recent_ms,
                      1000.0 / std::max(average_ms, 0.001),
                      1000.0 / std::max(one_percent_ms, 0.001));

  if (!detailed) {
    if (history.size() >= 2) {
      const std::vector<double> ordered = history.Ordered();
      frame_times_.resize(ordered.size());
      for (size_t i = 0; i < ordered.size(); ++i)
        frame_times_[i] = static_cast<float>(ordered[i]);

      const int plot_count = static_cast<int>(
          std::min<size_t>(frame_times_.size(), static_cast<size_t>(INT_MAX)));
      const float plot_max =
          std::max(static_cast<float>(budget_ms) * 1.6f,
                   static_cast<float>(history.Slowest()) * 1.1f);
      ImGui::PlotLines("##frame_time", frame_times_.data(), plot_count, 0,
                       nullptr, 0.0f, plot_max, ImVec2(-FLT_MIN, U(24.0f)));
    }

    ImGui::End();
    return;
  }

  ImGui::Separator();

  if (ImGui::BeginChild("##frame_monitor_body", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_None,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    ImGui::Spacing();
    ImGui::TextDisabled("Frame composition");

    const float radius = U(50.0f);
    const float available = ImGui::GetContentRegionAvail().x;
    if (available > radius * 2.0f)
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           (available - radius * 2.0f) * 0.5f);
    DrawPhaseDonut(timings, radius);

    ImGui::Spacing();
    DrawPhaseLegend(timings, ImGui::GetContentRegionAvail().x);

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextDisabled("Frame time");
    if (history.size() >= 2) {
      const std::vector<double> ordered = history.Ordered();
      frame_times_.resize(ordered.size());
      for (size_t i = 0; i < ordered.size(); ++i)
        frame_times_[i] = static_cast<float>(ordered[i]);

      const int plot_count = static_cast<int>(
          std::min<size_t>(frame_times_.size(), static_cast<size_t>(INT_MAX)));
      const float plot_max =
          std::max(static_cast<float>(budget_ms) * 1.6f,
                   static_cast<float>(history.Slowest()) * 1.1f);
      ImGui::PlotLines("##frame_time_curve", frame_times_.data(), plot_count, 0,
                       nullptr, 0.0f, plot_max, ImVec2(-FLT_MIN, U(48.0f)));
    } else {
      ImGui::TextDisabled("collecting...");
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Composition over time");
    DrawPhaseHistory(stats.timing_history, ImGui::GetContentRegionAvail().x,
                     U(52.0f), budget_ms);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Frame rate")) {
      ImGui::Text("average     %8.2f ms", average_ms);
      ImGui::Text("1%% low      %8.2f ms", one_percent_ms);
      ImGui::Text("0.1%% low    %8.2f ms", history.SlowAverage(0.001));
      ImGui::Text("fastest     %8.2f ms", history.Fastest());
      ImGui::Text("slowest     %8.2f ms", history.Slowest());
      ImGui::Text("budget      %8.2f ms  (%d fps)", budget_ms, target_rate);
      ImGui::TextDisabled("%u samples", static_cast<unsigned>(history.size()));
    }

    if (ImGui::CollapsingHeader("Counters")) {
      const RenderFrameCounters& counters = stats.last;
      ImGui::Text("bitmap ops  %8u",
                  static_cast<unsigned>(counters.bitmap_ops));
      ImGui::Text("bitmap cost %8.2f ms", counters.bitmap_total_ms());
      ImGui::Text("submits     %8u (%u bitmap)",
                  static_cast<unsigned>(counters.frame_submits),
                  static_cast<unsigned>(counters.bitmap_submits));
    }

    if (ImGui::CollapsingHeader("Device")) {
      const DeviceInfo& info = GPUDevice::Get().info();

      const auto text_or = [](const std::string& value, const char* fallback) {
        return value.empty() ? fallback : value.c_str();
      };

      ImGui::TextWrapped("GPU  %s", text_or(info.adapter_device, "unknown"));
      if (!info.adapter_vendor.empty() || info.vendor_id != 0) {
        ImGui::TextWrapped("Vendor  %s  (%04X:%04X)",
                           text_or(info.adapter_vendor, "unknown"),
                           static_cast<unsigned>(info.vendor_id),
                           static_cast<unsigned>(info.device_id));
      }
      if (!info.adapter_architecture.empty())
        ImGui::TextWrapped("Arch  %s", info.adapter_architecture.c_str());
      ImGui::Text("Backend  %s", text_or(info.backend, "unknown"));

      ImGui::Spacing();
      ImGui::Text("CPU cores  %d", info.cpu_cores);
      ImGui::Text("System RAM %llu MB",
                  static_cast<unsigned long long>(info.system_ram_mb));
    }
  }
  ImGui::EndChild();

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
  ExceptionState save_state;
  Config::Get().Save(save_state);
  if (save_state.HadException())
    LOGGER_ERROR("[ImGui] {}", save_state.message());
}

void ImGuiManager::ApplyUIScale() {
  ImGuiIO& io = ImGui::GetIO();
  ImGuiStyle& style = ImGui::GetStyle();

  const float logical_width =
      static_cast<float>(std::max(1, Graphics::Get().Width()));
  const float logical_height =
      static_cast<float>(std::max(1, Graphics::Get().Height()));
  const float scale =
      std::max(1.0f, std::min(io.DisplaySize.x / logical_width,
                              io.DisplaySize.y / logical_height));

  if (std::abs(scale - ui_scale_) < 0.01f)
    return;

  ui_scale_ = scale;
  LOGGER_INFO("debug UI scale {:.2f}", scale);

  style = BaseStyle();
  style.ScaleAllSizes(scale);
  style.FontScaleMain = scale;
}

}
