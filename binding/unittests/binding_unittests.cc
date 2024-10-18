// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binding/unittests/binding_unittests.h"

#include "SDL3_image/SDL_image.h"

namespace binding {

void BindingUnittests::InitializeBinding(
    scoped_refptr<content::EngineWorker> binding_host) {
  binding_ = binding_host;
}

void BindingUnittests::RunBindingMain() {
  scoped_refptr<content::Bitmap> bmp =
      new content::Bitmap(binding_->graphics(), binding_->io(), "test.png");
  scoped_refptr<content::Bitmap> bmp2 =
      new content::Bitmap(binding_->graphics(), binding_->io(), "bg.png");

  bmp2->Blt({100, 200}, bmp, {100, 100, 400, 400}, 255);

  auto* surf = bmp2->SurfaceRequired();
  IMG_SavePNG(surf, "out.png");

  while (true) {
    binding_->graphics()->Update();
  }
}

void BindingUnittests::FinalizeBinding() {}

void BindingUnittests::QuitRequired() {}

void BindingUnittests::ResetRequired() {}

}  // namespace binding
