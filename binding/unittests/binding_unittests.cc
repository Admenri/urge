// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binding/unittests/binding_unittests.h"

#include "content/public/plane.h"
#include "content/public/sprite.h"
#include "content/public/viewport.h"
#include "content/public/window.h"

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
  scoped_refptr<content::Bitmap> winskin = new content::Bitmap(
      binding_->graphics(), binding_->io(), "001-Blue01.png");

  bmp2->Blt({100, 200}, bmp, {100, 100, 400, 400}, 255);
  bmp2->SetPixel({10, 10}, new content::Color(255, 255, 255));

  bmp2->GradientFillRect({30, 30, 150, 150}, new content::Color(255, 0, 0),
                         new content::Color(0, 0, 255));

  bmp2->DrawText({100, 100, 100, 100}, "test draw text");

  auto* surf = bmp2->SurfaceRequired();
  IMG_SavePNG(surf, "out.png");

  binding_->graphics()->ResizeScreen({800, 600});

  scoped_refptr<content::Plane> spr2 = new content::Plane(binding_->graphics());
  spr2->SetBitmap(bmp);

  binding_->graphics()->Freeze();

  scoped_refptr<content::Viewport> vp =
      new content::Viewport(binding_->graphics(), {50, 50, 400, 400});
  vp->SetTone(new content::Tone(-68, -68, 0, 68));

  scoped_refptr<content::Sprite> spr =
      new content::Sprite(binding_->graphics(), vp);
  spr->SetBitmap(bmp2);
  spr->SetWaveAmp(10);

  binding_->graphics()->Transition(120);

  scoped_refptr<content::Window> win2 =
      new content::Window(binding_->graphics(), nullptr);
  win2->SetWindowskin(winskin);
  win2->SetX(100);
  win2->SetY(100);
  win2->SetWidth(200);
  win2->SetHeight(200);
  // win2->SetContents(bmp);
  win2->SetOX(100);
  win2->SetCursorRect(new content::Rect({10, 10, 100, 100}));
  win2->SetPause(true);

  int x = 0;
  while (true) {
    spr2->SetOX(x);
    spr2->SetOY(x);
    x += 5;
    spr->Update();

    win2->Update();
    binding_->graphics()->Update();
  }
}

void BindingUnittests::FinalizeBinding() {}

void BindingUnittests::QuitRequired() {}

void BindingUnittests::ResetRequired() {}

}  // namespace binding
