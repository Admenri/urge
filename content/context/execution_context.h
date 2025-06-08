// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_EXECUTION_CONTEXT_H_
#define CONTENT_CONTEXT_EXECUTION_CONTEXT_H_

#include <memory>

#include "components/filesystem/io_service.h"
#include "content/canvas/canvas_scheduler.h"
#include "content/canvas/font_context.h"
#include "content/context/disposable.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/render/drawable_controller.h"
#include "content/render/sprite_batch.h"
#include "content/worker/event_controller.h"
#include "renderer/context/render_context.h"
#include "renderer/device/render_device.h"
#include "ui/widget/widget.h"

namespace content {

struct ExecutionContext {
  base::Vec2i resolution;
  base::WeakPtr<ui::Widget> window;
  renderer::RenderDevice* render_device;
  renderer::RenderContext* primary_render_context;

  ContentProfile* engine_profile = nullptr;
  ScopedFontData* font_context = nullptr;
  I18NProfile* i18n_profile = nullptr;
  filesystem::IOService* io_service = nullptr;
  CanvasScheduler* canvas_scheduler = nullptr;
  SpriteBatch* sprite_batcher = nullptr;
  EventController* event_controller = nullptr;
  DisposableCollection* disposable_parent = nullptr;
  DrawNodeController* screen_drawable_node = nullptr;

  ExecutionContext();
  ~ExecutionContext();

  ExecutionContext(const ExecutionContext&) = delete;
  ExecutionContext& operator=(const ExecutionContext&) = delete;
};

}  // namespace content

#endif  //! CONTENT_CONTEXT_EXECUTION_CONTEXT_H_
