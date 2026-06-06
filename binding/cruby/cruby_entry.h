// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/binding/external_binding.h"

namespace binding {

class CRubyBindingEntry : public content::ExternalBinding {
 public:
  Result BindingInit() override;
  Result RunningIterate() override;
  void BindingQuit() override;
};

}  // namespace binding
