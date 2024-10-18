// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_FLASHABLE_H_
#define CONTENT_PUBLIC_FLASHABLE_H_

#include "content/public/utility.h"

namespace content {

class Flashable {
 public:
  Flashable() = default;

  Flashable(const Flashable&) = delete;
  Flashable& operator=(const Flashable&) = delete;

  void Flash(scoped_refptr<Color> flash_color, int duration) {
    if (duration <= 0)
      return;

    empty_flashing_ = !flash_color;
    flash_color_ = flash_color ? flash_color->AsBase() : base::Vec4();
    flash_alpha_ = flash_color_.w;
    flash_duration_ = duration;
    is_flashing_ = true;
    count_ = 0;
  }

  virtual void Update() {
    if (!is_flashing_)
      return;

    if (++count_ > flash_duration_) {
      is_flashing_ = false;
      flash_color_ = base::Vec4();
      empty_flashing_ = false;
      return;
    }

    flash_color_.w =
        flash_alpha_ * (1.0f - static_cast<float>(count_) / flash_duration_);
  }

 protected:
  base::Vec4& GetFlashColor() { return flash_color_; }
  bool IsFlashing() { return is_flashing_; }
  bool EmptyFlashing() { return empty_flashing_; }

 private:
  bool is_flashing_ = false;
  bool empty_flashing_ = false;
  base::Vec4 flash_color_;
  int flash_duration_ = 0;
  int count_ = 0;
  float flash_alpha_ = 0.0f;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_FLASHABLE_H_