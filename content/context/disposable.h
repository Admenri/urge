// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_DISPOSABLE_H_
#define CONTENT_CONTEXT_DISPOSABLE_H_

#include <stdint.h>
#include <string>

#include "base/bind/callback_list.h"
#include "base/buildflags/build.h"
#include "base/containers/linked_list.h"
#include "content/context/exception_state.h"

namespace content {

class Disposable;

class DisposableCollection {
 public:
  virtual void AddDisposable(Disposable* disp) = 0;
};

class Disposable : public base::LinkNode<Disposable> {
 public:
  Disposable(DisposableCollection* parent);
  virtual ~Disposable();

  Disposable(const Disposable&) = delete;
  Disposable& operator=(const Disposable&) = delete;

  void Dispose(ExceptionState& exception_state);
  bool IsDisposed(ExceptionState& exception_state);

  bool CheckDisposed(ExceptionState& exception_state);

 protected:
  virtual void OnObjectDisposed() = 0;
  virtual base::String DisposedObjectName() = 0;

 private:
  int32_t disposed_;
};

}  // namespace content

#endif  //! CONTENT_COMMON_DISPOSABLE_H_
