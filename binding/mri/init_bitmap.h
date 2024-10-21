// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_INIT_BITMAP_H_
#define BINDING_MRI_INIT_BITMAP_H_

#include "binding/mri/mri_util.h"
#include "content/public/bitmap.h"

namespace binding {

MRI_DECLARE_DATATYPE(Bitmap);

void bitmap_init_prop(scoped_refptr<content::Bitmap> bitmap, VALUE self);

void InitBitmapBinding();

}  // namespace binding

#endif  //! BINDING_MRI_INIT_BITMAP_H_
