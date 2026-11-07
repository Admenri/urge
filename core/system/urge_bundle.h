// Licensed under the MIT License. SPDX-License-Identifier: MIT
// Copyright (c) 2026 URGE contributors.
#pragma once

#include <string>

namespace urge::urge_bundle {

bool Register();
// Applies to subsequent mounts. Empty clears the password. Existing mounts
// retain their derived keys, so bundles with different passwords can coexist.
bool SetPassword(const std::string& password);

}  // namespace urge::urge_bundle
