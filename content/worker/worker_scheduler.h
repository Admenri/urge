// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_SCHEDULER_H_
#define CONTENT_WORKER_WORKER_SCHEDULER_H_

#include "content/worker/engine_worker.h"
#include "fiber/fiber.h"

namespace content {

struct ContentParams {
  // Engine global configure
  scoped_refptr<CoreConfigure> config;

  // Renderer target host
  std::unique_ptr<ui::Widget> window;

  // Global filesystem
  filesystem::Filesystem* file_io;

  // Binding entry
  std::unique_ptr<BindingEngine> binding;
};

class WorkerScheduler {
 public:
  WorkerScheduler();
  ~WorkerScheduler();

  WorkerScheduler(const WorkerScheduler&) = delete;
  WorkerScheduler& operator=(const WorkerScheduler&) = delete;

  void Init(ContentParams init_params);
  void Run();

 private:
  scoped_refptr<EngineWorker> engine_worker_;
};

}  // namespace content

#endif  //! CONTENT_WORKER_WORKER_SCHEDULER_H_
