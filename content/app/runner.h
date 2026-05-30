// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "SDL3/SDL_events.h"

#include "content/binding/external_binding.h"
#include "content/profile/core_profile.h"
#include "ui/widget/widget.h"

namespace content {

class Runner {
 public:
  Runner(std::unique_ptr<ExternalBinding> binding_entry);
  ~Runner();

  Runner(const Runner&) = delete;
  Runner& operator=(const Runner&) = delete;

  // On application initialization
  ExternalBinding::Result AppInit();

  // Run one frame
  ExternalBinding::Result RunIterate();

  // Input event
  ExternalBinding::Result ProcessEvent(SDL_Event* event);

 private:
  std::unique_ptr<ExternalBinding> binding_entry_;
};

}  // namespace content
