// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_INIT_UTILITY_H_
#define BINDING_MRI_INIT_UTILITY_H_

#include "binding/mri/mri_util.h"

namespace binding {

MRI_DECLARE_DATATYPE(Rect);
MRI_DECLARE_DATATYPE(Color);
MRI_DECLARE_DATATYPE(Tone);

void InitUtilityBinding();

}  // namespace binding

#endif  // !BINDING_MRI_INIT_UTILITY_H_
