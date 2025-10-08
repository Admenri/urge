// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_PLATFORM_MRI_EMSCRIPTEN_H_
#define BINDING_MRI_PLATFORM_MRI_EMSCRIPTEN_H_

#include "base/buildflags/build.h"

#if defined(OS_EMSCRIPTEN)
namespace binding {

void InitEmscriptenBinding();

}  // namespace binding
#endif  // OS_EMSCRIPTEN

#endif  //! BINDING_MRI_PLATFORM_MRI_EMSCRIPTEN_H_
