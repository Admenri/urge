// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/binding/external_binding.h"

namespace content {

ExternalBinding::Result ExternalBinding::BindingInit() {
  return Result::CONTINUE;
}

ExternalBinding::Result ExternalBinding::RunningIterate() {
  return Result::CONTINUE;
}

void ExternalBinding::BindingQuit() {}

}  // namespace content
