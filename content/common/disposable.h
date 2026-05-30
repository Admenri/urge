// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string_view>

#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"

namespace content {

URGE_BINDING()
class Disposable : public Object {
 public:
  Disposable() : Object(), disposed_(false) {}

  Disposable(const Disposable&) = delete;
  Disposable& operator=(const Disposable&) = delete;

  // Disposed check
  bool IsDisposed() const { return disposed_; }

  // Using in destruction
  void Dispose() {
    if (!disposed_)
      OnObjectRelease();
    disposed_ = true;
  }

 public:
  URGE_BINDING()
  bool IsDisposed(URGE_EXCEPTION) { return disposed_; }

  URGE_BINDING()
  void Dispose(URGE_EXCEPTION) { Dispose(); }

 protected:
  virtual std::string_view ObjectName() = 0;
  virtual void OnObjectRelease() = 0;

 private:
  bool disposed_;
};

}  // namespace content
