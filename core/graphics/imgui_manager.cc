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

#include "core/graphics/imgui_manager.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <iterator>
#include <string>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_misc.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

#include "core/audio/audio.h"
#include "core/system/config.h"
#include "core/graphics/device.h"
#include "core/font/font_context.h"
#include "core/graphics/graphics.h"
#include "core/input/input.h"
#include "core/common/logger.h"
#include "core/graphics/render_stats.h"

#include <cstdio>
#include <cfloat>

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

}  // namespace

ImGuiManager::ImGuiManager(SDL_Window* window) : window_(window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigWindowsMoveFromTitleBarOnly = true;

  // `fpsmonitor` opens the frame monitor in its detailed view straight away,
  // which saves two F2 presses when the reason for launching is to profile.
  // The token follows the engine's other switches -- `bstats`, `nojit` -- and
  // is matched bare, exactly as written on the command line.
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

  /* The overlay draws with the same face as the game instead of opening a file
     of its own: on Windows both land on Microsoft YaHei, and a project that
     ships its own `Fonts/` faces still gets them through the same lookup.  The
     payload belongs to the font context, which outlives this manager, so the
     atlas only borrows it. */
  bool font_loaded = false;
  const std::string& ui_font = FontContext::Get().default_font();
  if (const FontContext::FontData* data = FontContext::Get().FindFont(ui_font)) {
    ImFontConfig config;
    config.SizePixels = kUiFontSize;
    config.FontDataOwnedByAtlas = false;

    if (io.Fonts->AddFontFromMemoryTTF(data->data,
                                       static_cast<int>(data->size),
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

  /* A pad button is a capture candidate like a key; it never toggles the
     overlay itself, so this returns either way. */
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
  // hidden -> compact -> detailed -> hidden.  The window keeps its corner and
  // only changes its content, so cycling through the modes feels like
  // unfolding one window rather than opening a new one each time.
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

  /* Which controller the entries below belong to -- or none, which is also the
     answer when the pad is unplugged and its bindings are simply idle.  The
     slot bindings always follow the first pad; the extra count is there so a
     second controller does not look like it was ignored. */
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
          /* Keyboard and pad number their controls independently, so the id
             needs both halves -- `A` on the pad and the `A` key share a code. */
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

void ImGuiManager::BuildFPSWindow() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const bool detailed = fps_mode_ == FpsMode::kDetailed;

  // The window is pinned to the top-right corner and must never grow past the
  // display: a game window can be dragged down to a handful of pixels, and the
  // overlay has to stay readable (and reachable) there too.
  //
  // The size is computed here and set outright rather than left to
  // `AlwaysAutoResize`.  Auto-resize is driven by the content's ideal size,
  // and that content contains a plot whose width follows the window -- so the
  // two feed each other, ImGui re-runs the fit after the constraint pass with
  // no damper (`SizeFull.y` is assigned, not clamped, while auto-fitting), and
  // the window grows without bound until the scroll region overflows.
  // A fixed size breaks that loop: the height is whatever the current mode
  // wants, clamped to the display, and the detail list scrolls inside it.
  const ImVec2 display = viewport->Size;
  const float margin = U(12.0f);
  const float desired_width = U(detailed ? 336.0f : 210.0f);
  const float width =
      std::max(U(120.0f), std::min(desired_width, display.x - margin * 2.0f));

  // A short window must not push the overlay off-screen, so both axes are
  // clamped to what the display actually has.
  const float min_height = U(60.0f);
  const float max_height = std::max(min_height, display.y - margin * 2.0f);

  // The height is the mode's natural size clamped to the display, so the
  // overlay is compact in compact mode and only as tall as the detail needs
  // otherwise.  `SetNextWindowSize` with a concrete height replaces what
  // `AlwaysAutoResize` used to do, without the feedback loop.
  const float desired_height = U(detailed ? 300.0f : 92.0f);
  const float height = std::max(min_height, std::min(desired_height, max_height));

  // Both the position and the size are set every frame.  The corner anchor
  // depends on the width, so a window that keeps a stale position while its
  // width is re-set would drift off the corner; the overlay is draggable-free
  // by design (NoMove is not set, but the user has no reason to move it, and
  // re-pinning is what keeps it glued to the corner when the window resizes).
  ImGui::SetNextWindowPos(
      ImVec2(viewport->Pos.x + display.x - width - margin,
             viewport->Pos.y + margin),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;

  if (!ImGui::Begin("Frame monitor", nullptr, flags)) {
    ImGui::End();
    return;
  }

  const RenderStats& stats = GetRenderStats();
  const FrameTimeHistory& history = stats.frame_history;

  const ImVec4 kGood(0.42f, 0.85f, 0.45f, 1.0f);
  const ImVec4 kWarn(0.95f, 0.78f, 0.28f, 1.0f);

  const auto rate_text = [](double ms, char* buffer, size_t size) {
    if (ms <= 0.0)
      snprintf(buffer, size, "  -- ");
    else
      snprintf(buffer, size, "%5.1f", 1000.0 / ms);
  };

  // --- Rates, always shown: the compact view is exactly this block. --------
  const double last_ms = history.last();
  const double average_ms = history.Average();
  const double one_percent_ms = history.SlowAverage(0.01);

  char last_rate[32], average_rate[32], low_rate[32];
  rate_text(last_ms, last_rate, sizeof(last_rate));
  rate_text(average_ms, average_rate, sizeof(average_rate));
  rate_text(one_percent_ms, low_rate, sizeof(low_rate));

  ImGui::TextUnformatted("FPS");
  ImGui::SameLine();
  ImGui::TextColored(last_ms > 0.0 && last_ms <= 1000.0 / 50.0 ? kGood : kWarn,
                     "%s", last_rate);
  ImGui::SameLine();
  ImGui::TextDisabled("| %.2f ms", last_ms);

  ImGui::Text("avg %s   %.2f ms", average_rate, average_ms);
  ImGui::Text("1%% low %s", low_rate);

  if (!detailed) {
    ImGui::End();
    return;
  }

  // --- Detailed view -------------------------------------------------------
  ImGui::Separator();

  if (history.size() >= 2) {
    // `PlotLines` wants a contiguous float array in display order, and the
    // history keeps its samples ring-buffered, so the copy is unavoidable --
    // but it only happens while the detailed view is on screen.  The count is
    // clamped to `INT_MAX` rather than cast straight from `size_t`: the window
    // is fixed at 240 samples so it never gets close, but an overflowing cast
    // would hand the plotter a negative count and read out of bounds.
    const std::vector<double> ordered = history.Ordered();
    frame_times_.resize(ordered.size());
    for (size_t i = 0; i < ordered.size(); ++i)
      frame_times_[i] = static_cast<float>(ordered[i]);
    const int plot_count = static_cast<int>(
        std::min<size_t>(frame_times_.size(), static_cast<size_t>(INT_MAX)));
    const float plot_max =
        std::max(1000.0f / 30.0f,
                 static_cast<float>(history.Slowest()) * 1.15f);
    ImGui::PlotLines("##frame_time", frame_times_.data(), plot_count, 0, "ms",
                     0.0f, plot_max, ImVec2(-FLT_MIN, U(46.0f)));
  } else {
    ImGui::TextDisabled("collecting...");
  }

  // Everything below keeps its own scroll region, so a window that was shrunk
  // down shows the top of the list rather than rendering the tail off-screen.
  const float body_height = ImGui::GetContentRegionAvail().y;
  if (body_height > 0.0f) {
    ImGui::BeginChild("##details", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (ImGui::CollapsingHeader("Frame rate",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("average     %8.2f ms", average_ms);
      ImGui::Text("1%% low      %8.2f ms", one_percent_ms);
      ImGui::Text("0.1%% low    %8.2f ms", history.SlowAverage(0.001));
      ImGui::Text("fastest     %8.2f ms", history.Fastest());
      ImGui::Text("slowest     %8.2f ms", history.Slowest());
      ImGui::TextDisabled("%u samples",
                          static_cast<unsigned>(history.size()));
    }

    if (ImGui::CollapsingHeader("Frame time",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      const FrameTimings& t = stats.timings;
      ImGui::Text("cpu total   %8.2f ms", t.cpu_ms);
      ImGui::Text("  script    %8.2f ms", t.script_ms);
      ImGui::Text("  layout    %8.2f ms", t.layout_ms);
      ImGui::Text("  render    %8.2f ms", t.render_ms);
      ImGui::Text("  update    %8.2f ms", t.update_ms);
      ImGui::Text("  other     %8.2f ms", t.other_ms);
      ImGui::Text("present     %8.2f ms", t.present_ms);

      ImGui::Spacing();
      const RenderFrameCounters& counters = stats.last;
      ImGui::Text("bitmap ops  %8u", static_cast<unsigned>(counters.bitmap_ops));
      ImGui::Text("bitmap cost %8.2f ms", counters.bitmap_total_ms());
      ImGui::Text("submits     %8u (%u bitmap)",
                  static_cast<unsigned>(counters.frame_submits),
                  static_cast<unsigned>(counters.bitmap_submits));
    }

    if (ImGui::CollapsingHeader("Device",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
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

    ImGui::EndChild();
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

}  // namespace urge
