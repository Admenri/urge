// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_ENGINE_WORKER_H_
#define CONTENT_WORKER_ENGINE_WORKER_H_

#include "base/memory/ref_counted.h"
#include "content/binding/binding_engine.h"
#include "content/common/content_utils.h"
#include "content/config/engine_config.h"
#include "content/public/graphics.h"
#include "content/public/input.h"
#include "content/public/mouse.h"
#include "content/public/touch.h"
#include "ui/widget/widget.h"

namespace content {

class EngineWorker : public base::RefCounted<EngineWorker> {
 public:
  EngineWorker();
  ~EngineWorker();

  EngineWorker(const EngineWorker&) = delete;
  EngineWorker& operator=(const EngineWorker&) = delete;

  void InitEngineMain(scoped_refptr<CoreConfigure> config,
                      std::unique_ptr<ui::Widget> window,
                      filesystem::Filesystem* io,
                      std::unique_ptr<BindingEngine> binding);
  bool RunMainLoop();

  filesystem::Filesystem* io() { return io_; }
  scoped_refptr<CoreConfigure> config() { return config_; }

  base::WeakPtr<ui::Widget> window() { return window_->AsWeakPtr(); }
  scoped_refptr<Graphics> graphics() { return graphics_; }
  scoped_refptr<Input> input() { return input_; }
  scoped_refptr<Mouse> mouse() { return mouse_; }
  scoped_refptr<Touch> touch() { return touch_; }

 private:
  static void EngineMainLoopFunc(fiber_t* current_coroutine);

  std::unique_ptr<BindingEngine> binding_engine_;
  scoped_refptr<CoreConfigure> config_;
  filesystem::Filesystem* io_;

  std::unique_ptr<CoroutineContext> cc_;

  std::unique_ptr<ui::Widget> window_;
  scoped_refptr<Graphics> graphics_;
  scoped_refptr<Input> input_;
  scoped_refptr<Mouse> mouse_;
  scoped_refptr<Touch> touch_;
};

}  // namespace content

#endif  //! CONTENT_WORKER_ENGINE_WORKER_H_
