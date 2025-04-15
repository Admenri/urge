// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_CANVAS_SCHEDULER_H_
#define CONTENT_CANVAS_CANVAS_SCHEDULER_H_

#include "base/worker/thread_worker.h"
#include "components/filesystem/io_service.h"
#include "content/canvas/canvas_impl.h"
#include "renderer/device/render_device.h"
#include "renderer/resource/render_buffer.h"

namespace content {

class CanvasScheduler {
 public:
  ~CanvasScheduler();

  CanvasScheduler(const CanvasScheduler&) = delete;
  CanvasScheduler& operator=(const CanvasScheduler&) = delete;

  static std::unique_ptr<CanvasScheduler> MakeInstance(
      renderer::RenderDevice* device,
      filesystem::IOService* io_service);

  renderer::RenderDevice* GetDevice();
  filesystem::IOService* GetIO();

  // Bind a worker for current scheduler,
  // all bitmap/canvas draw command will be encoded on this worker.
  // If worker set to null, it will be executed immediately on caller thread.
  void InitWithRenderWorker(base::ThreadWorker* worker);

  // Sync all pending command to device queue,
  // clear children canvas command queue.
  void SubmitPendingPaintCommands();

  base::ThreadWorker* render_worker() { return render_worker_; }
  renderer::QuadBatch* quad_batch() { return common_quad_batch_.get(); }
  std::vector<uint8_t>* text_render_buffer() { return &text_render_buffer_; }

  renderer::Binding_Base* base_binding() { return generic_base_binding_.get(); }
  renderer::Binding_Color* color_binding() {
    return generic_color_binding_.get();
  }

 private:
  friend class CanvasImpl;
  CanvasScheduler(renderer::RenderDevice* device,
                  filesystem::IOService* io_service);

  base::LinkedList<CanvasImpl> children_;

  renderer::RenderDevice* device_;
  base::ThreadWorker* render_worker_;
  filesystem::IOService* io_service_;
  std::vector<uint8_t> text_render_buffer_;

  std::unique_ptr<renderer::Binding_Base> generic_base_binding_;
  std::unique_ptr<renderer::Binding_Color> generic_color_binding_;

  std::unique_ptr<renderer::QuadBatch> common_quad_batch_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_CANVAS_SCHEDULER_H_
