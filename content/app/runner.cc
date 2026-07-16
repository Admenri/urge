// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/runner.h"

#include <map>

#include "content/profile/core_profile.h"
#include "content/render/graphics.h"

namespace content {

namespace {

const std::map<std::string, wgpu::BackendType> kRendererBackends = {
    {"D3D11", wgpu::BackendType::D3D11},    {"D3D12", wgpu::BackendType::D3D12},
    {"VULKAN", wgpu::BackendType::Vulkan},  {"OGL", wgpu::BackendType::OpenGL},
    {"OGLES", wgpu::BackendType::OpenGLES},
};

}

Runner::Runner(std::unique_ptr<ExternalBinding> binding_entry)
    : binding_entry_(std::move(binding_entry)) {}

Runner::~Runner() {
  // Release runner resource
  binding_entry_->BindingQuit();

  // Destroy component
  Graphics::Instance(nullptr);
}

ExternalBinding::Result Runner::AppInit() {
  auto* core_profile = CoreProfile::Instance();

  // Main window
  ui::Widget::InitParams window_params;
  window_params.size = core_profile->window.size;
  window_params.title = core_profile->window.title;
  window_params.resizable = core_profile->window.resizable;

  auto window = std::make_unique<ui::Widget>();
  window->Init(std::move(window_params));

  // Primary rendering device
  auto graphics_device = renderer::RenderDevice::Create(window->AsWeakPtr());

  // Graphics component
  Graphics::Instance(
      new Graphics(std::move(window), std::move(graphics_device)));

  return binding_entry_->BindingInit();
}

ExternalBinding::Result Runner::RunIterate() {
  // Binding iterate
  auto result = binding_entry_->RunningIterate();

  // Graphics update
  Graphics::Instance()->Present();

  return result;
}

ExternalBinding::Result Runner::ProcessEvent(SDL_Event* event) {
  if (event->type == SDL_EVENT_QUIT)
    return ExternalBinding::Result::SUCCESS;

  // GUI Event
  ui::IMGUIContext::ProcessEvent(event);

  return ExternalBinding::Result::CONTINUE;
}

}  // namespace content
