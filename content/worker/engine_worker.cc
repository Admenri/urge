// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/worker/engine_worker.h"

namespace content {

EngineWorker::EngineWorker() : io_(nullptr) {}

EngineWorker::~EngineWorker() {}

void EngineWorker::InitEngineMain(scoped_refptr<CoreConfigure> config,
                                  std::unique_ptr<ui::Widget> window,
                                  filesystem::Filesystem* io,
                                  std::unique_ptr<BindingEngine> binding) {
  config_ = config;
  io_ = io;
  binding_engine_ = std::move(binding);
  window_ = std::move(window);

  // Default font settings
  std::unique_ptr<ScopedFontData> default_font(
      new ScopedFontData(io, config_->default_font_path()));

  // Init kernel coroutine context
  cc_.reset(new CoroutineContext);
  cc_->primary_fiber = fiber_create(nullptr, 0, nullptr, nullptr);
  cc_->main_loop_fiber =
      fiber_create(cc_->primary_fiber, 0, EngineMainLoopFunc, this);

  // Init Modules
  graphics_ =
      new Graphics(cc_.get(), window_->AsWeakPtr(), std::move(default_font),
                   config_->initial_resolution(), config_->content_version());
  input_ = new Input(window_->AsWeakPtr(), config_->content_version());
  mouse_ = new Mouse(window_->AsWeakPtr());
  touch_ = new Touch(window_->AsWeakPtr());
}

bool EngineWorker::RunMainLoop() {
  if (!binding_engine_)
    return false;

  return graphics_->ExecuteEventMainLoop();
}

void EngineWorker::EngineMainLoopFunc(fiber_t* current_coroutine) {
  // Self
  EngineWorker* self = static_cast<EngineWorker*>(current_coroutine->userdata);

  // Before run main initialize
  self->binding_engine_->InitializeBinding(self);

  // Boot binding components
  self->binding_engine_->RunBindingMain();

  // Destroy and release resource for current worker cc
  self->binding_engine_->FinalizeBinding();
  self->binding_engine_.reset();

  // Switch fiber
  fiber_switch(self->cc_->primary_fiber);
}

}  // namespace content
