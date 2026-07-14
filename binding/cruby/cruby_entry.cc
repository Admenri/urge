// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binding/cruby/cruby_entry.h"

#include "content/profile/core_profile.h"

namespace binding {

content::ExternalBinding::Result CRubyBindingEntry::BindingInit() {
  auto* profile = content::CoreProfile::Instance();
}

content::ExternalBinding::Result CRubyBindingEntry::RunningIterate() {}

void CRubyBindingEntry::BindingQuit() {}

}  // namespace binding
