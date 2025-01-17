// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SDL3/SDL_main.h"

#include "binding/unittests/engine_binding_unittests.h"
#include "content/worker/content_runner.h"

int SDL_main(int argc, char* argv[]) {
  std::unique_ptr<content::ContentProfile> profile =
      content::ContentProfile::MakeFrom(nullptr);

  std::unique_ptr<ui::Widget> widget(new ui::Widget);
  ui::Widget::InitParams widget_params;
  widget_params.size = base::Vec2i(640, 480);
  widget->Init(std::move(widget_params));

  content::ContentRunner::InitParams content_params;
  content_params.profile = std::move(profile);
  content_params.window = widget->AsWeakPtr();
  content_params.entry = std::make_unique<EngineBindingUnittests>();

  std::unique_ptr<content::ContentRunner> runner =
      content::ContentRunner::Create(std::move(content_params));
  for (;;)
    if (runner->RunMainLoop())
      break;

  return 0;
}
