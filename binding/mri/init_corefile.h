// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_INIT_COREFILE_H_
#define BINDING_MRI_INIT_COREFILE_H_

#include "binding/mri/mri_util.h"

#include <string>

namespace binding {

VALUE MriLoadData(const std::string& filename, bool mri_exc);

void InitCoreFileBinding();

}  // namespace binding

#endif  //! BINDING_MRI_INIT_COREFILE_H_
