// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#pragma once

#include "binding/mri/mri_util.h"
#include "content/content_config.h"

#include <string>

namespace binding {

VALUE MriLoadData(const std::string& filename, URGE_EXCEPTION);

void InitCoreFileBinding();

}  // namespace binding
