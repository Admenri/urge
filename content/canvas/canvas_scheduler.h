// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_CANVAS_SCHEDULER_H_
#define CONTENT_CANVAS_CANVAS_SCHEDULER_H_

#include "base/worker/thread_worker.h"
#include "components/filesystem/io_service.h"
#include "content/canvas/canvas_impl.h"
#include "renderer/context/render_context.h"
#include "renderer/device/render_device.h"
#include "renderer/resource/render_buffer.h"

namespace content {

class CanvasScheduler {
 public:
  // All bitmap/canvas draw command will be encoded on this worker.
  // If worker set to null, it will be executed immediately on caller thread.
  CanvasScheduler(base::ThreadWorker* worker,
                  renderer::RenderDevice* device,
                  renderer::RenderContext* context,
                  filesystem::IOService* io_service);
  ~CanvasScheduler();

  CanvasScheduler(const CanvasScheduler&) = delete;
  CanvasScheduler& operator=(const CanvasScheduler&) = delete;

  renderer::RenderDevice* GetDevice() const;
  renderer::RenderContext* GetContext() const;
  filesystem::IOService* GetIOService() const;

  // Sync all pending command to device queue,
  // clear children canvas command queue.
  void SubmitPendingPaintCommands();

  // Setup canvas render target
  // Cached render state
  void SetupRenderTarget(Diligent::ITextureView* render_target,
                         bool clear_target);

  base::ThreadWorker* render_worker() { return render_worker_; }
  renderer::QuadBatch* quad_batch() { return common_quad_batch_.get(); }

  renderer::Binding_Base* base_binding() { return generic_base_binding_.get(); }
  renderer::Binding_Color* color_binding() {
    return generic_color_binding_.get();
  }

 private:
  friend class CanvasImpl;

  base::LinkedList<CanvasImpl> children_;

  renderer::RenderDevice* device_;
  renderer::RenderContext* context_;
  base::ThreadWorker* render_worker_;
  filesystem::IOService* io_service_;

  base::OwnedPtr<renderer::Binding_Base> generic_base_binding_;
  base::OwnedPtr<renderer::Binding_Color> generic_color_binding_;

  base::OwnedPtr<renderer::QuadBatch> common_quad_batch_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_CANVAS_SCHEDULER_H_
