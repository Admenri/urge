// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_MRI_FILE_H_
#define BINDING_MRI_MRI_FILE_H_

#include "binding/mri/mri_util.h"

#include <string>

namespace binding {

VALUE MriLoadData(const std::string& filename,
                  content::ExceptionState& exception_state);

void InitCoreFileBinding();

}  // namespace binding

#endif  //! BINDING_MRI_INIT_COREFILE_H_
