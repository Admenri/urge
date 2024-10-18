// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/binding/binding_engine.h"

#include "content/common/content_utils.h"
#include "content/worker/engine_worker.h"

namespace content {

BindingEngine::BindingEngine() {}

BindingEngine::~BindingEngine() {}

void BindingEngine::InitializeBinding(
    scoped_refptr<EngineWorker> binding_host) {
  Debug() << "Empty InitializeBinding implement for BindingEngine.";
}

void BindingEngine::RunBindingMain() {
  Debug() << "Empty RunBindingMain implement for BindingEngine.";
}

void BindingEngine::FinalizeBinding() {
  Debug() << "Empty FinalizeBinding implement for BindingEngine.";
}

void BindingEngine::QuitRequired() {
  Debug() << "Empty QuitRequired implement for BindingEngine.";
}

void BindingEngine::ResetRequired() {
  Debug() << "Empty ResetRequired implement for BindingEngine.";
}

}  // namespace content
