// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/public/touch.h"

namespace content {

Touch::Touch(base::WeakPtr<ui::Widget> window) : window_(window) {}

double Touch::GetHorizontalFinger(int fid) {
  if (fid < 0 || fid >= GetMaxFingersNum())
    return 0;
  return window_->GetTouchState()[fid].x;
}

double Touch::GetVerticalFinger(int fid) {
  if (fid < 0 || fid >= GetMaxFingersNum())
    return 0;
  return window_->GetTouchState()[fid].y;
}

bool Touch::IsFingerPressed(int fid) {
  if (fid < 0 || fid >= GetMaxFingersNum())
    return false;
  return window_->GetTouchState()[fid].down;
}

}  // namespace content
