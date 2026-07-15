// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/context/imgui_context.h"

#include "SDL3/SDL_video.h"
#include "imgui/imgui.h"

#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_wgpu.h"

namespace ui {

IMGUIContext::IMGUIContext(renderer::RenderDevice* render_device,
                           wgpu::TextureFormat target_format) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  // Docking
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Setup Dear ImGui style
  ImGui::StyleColorsClassic();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  // Bake a fixed style scale. (until we have a solution for
  // dynamic style scaling, changing this requires resetting
  // Style + calling this again)
  style.ScaleAllSizes(main_scale);
  // Set initial font scale. (in docking branch: using
  // io.ConfigDpiScaleFonts=true automatically overrides this
  // for every window depending on the current monitor)
  style.FontScaleDpi = main_scale;

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForOther(render_device->window()->AsSDLWindow());

  wgpu::SurfaceCapabilities surface_capabilities;
  render_device->swapchain().GetCapabilities(render_device->adapter(),
                                             &surface_capabilities);

  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = render_device->device().Get();
  init_info.RenderTargetFormat = static_cast<WGPUTextureFormat>(target_format);
  ImGui_ImplWGPU_Init(&init_info);
}

IMGUIContext::~IMGUIContext() {
  // Cleanup
  ImGui_ImplWGPU_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

// static
void IMGUIContext::SetupFrame() {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplSDL3_NewFrame();
}

// static
void IMGUIContext::ProcessEvent(const SDL_Event* event) {
  ImGui_ImplSDL3_ProcessEvent(event);
}

// static
void IMGUIContext::Render(ImDrawData* draw_data,
                          const wgpu::RenderPassEncoder& pass) {
  ImGui_ImplWGPU_RenderDrawData(draw_data, pass.Get());
}

}  // namespace ui
