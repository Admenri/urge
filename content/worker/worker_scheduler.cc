// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/worker/worker_scheduler.h"

namespace content {

WorkerScheduler::WorkerScheduler() {}

WorkerScheduler::~WorkerScheduler() {}

void WorkerScheduler::Init(ContentParams init_params) {
  // Main worker
  engine_worker_ = new EngineWorker;
  engine_worker_->InitEngineMain(
      init_params.config, std::move(init_params.window), init_params.file_io,
      std::move(init_params.binding));
}

void WorkerScheduler::Run() {
  // Run in coroutine mode
  while (true) {
    if (!engine_worker_->RunMainLoop())
      break;
  }
}

}  // namespace content
