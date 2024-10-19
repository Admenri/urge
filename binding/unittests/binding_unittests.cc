// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binding/unittests/binding_unittests.h"

#include "content/public/sprite.h"
#include "content/public/viewport.h"

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
  bmp2->SetPixel({10, 10}, new content::Color(255, 255, 255));

  bmp2->GradientFillRect({30, 30, 150, 150}, new content::Color(255, 0, 0),
                         new content::Color(0, 0, 255));

  bmp2->DrawText({100, 100, 100, 100}, "test draw text");

  auto* surf = bmp2->SurfaceRequired();
  IMG_SavePNG(surf, "out.png");

  scoped_refptr<content::Viewport> vp =
      new content::Viewport(binding_->graphics(), {50, 50, 400, 400});
  vp->SetTone(new content::Tone(-68, -68, 0, 68));

  scoped_refptr<content::Sprite> spr =
      new content::Sprite(binding_->graphics(), vp);
  spr->SetBitmap(bmp2);
  spr->SetX(100);
  spr->SetY(100);
  spr->SetOX(bmp2->GetSize().x / 2);
  spr->SetOY(bmp2->GetSize().y / 2);
  spr->SetWaveAmp(10);

  while (true) {
    spr->Update();
    binding_->graphics()->Update();
  }
}

void BindingUnittests::FinalizeBinding() {}

void BindingUnittests::QuitRequired() {}

void BindingUnittests::ResetRequired() {}

}  // namespace binding
