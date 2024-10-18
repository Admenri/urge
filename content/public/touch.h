// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TOUCH_H_
#define CONTENT_PUBLIC_TOUCH_H_

#include "base/memory/ref_counted.h"
#include "ui/widget/widget.h"

namespace content {

class Touch : public base::RefCounted<Touch> {
 public:
  Touch(base::WeakPtr<ui::Widget> window);

  Touch(const Touch&) = delete;
  Touch& operator=(const Touch&) = delete;

  int GetMaxFingersNum() const { return MAX_FINGERS; }
  double GetHorizontalFinger(int fid);
  double GetVerticalFinger(int fid);
  bool IsFingerPressed(int fid);

 private:
  base::WeakPtr<ui::Widget> window_;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_TOUCH_H_
