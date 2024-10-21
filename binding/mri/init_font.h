// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_INIT_FONT_H_
#define BINDING_MRI_INIT_FONT_H_

#include "binding/mri/mri_util.h"

namespace binding {

MRI_DECLARE_DATATYPE(Font);

void font_init_prop(scoped_refptr<content::Font> font, VALUE self);

void InitFontBinding();

}  // namespace binding

#endif  //! BINDING_MRI_INIT_FONT_H_
