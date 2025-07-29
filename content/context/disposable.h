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

#define DISPOSE_CHECK_RETURN(v)       \
  if (CheckDisposed(exception_state)) \
    return v;

#define DISPOSE_CHECK                 \
  if (CheckDisposed(exception_state)) \
    return;

#define DISPOSABLE_DEFINITION(klass)        \
  klass::~klass() {                         \
    Disposable::Dispose();                  \
  }                                         \
  void klass::Dispose(ExceptionState&) {    \
    Disposable::Dispose();                  \
  }                                         \
  bool klass::IsDisposed(ExceptionState&) { \
    return Disposable::IsDisposed();        \
  }

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

  void Dispose();
  bool IsDisposed() const { return disposed_; }

 protected:
  virtual void OnObjectDisposed() = 0;
  virtual std::string DisposedObjectName() = 0;

  bool CheckDisposed(ExceptionState& exception_state);

 private:
  int32_t disposed_;
};

}  // namespace content

#endif  //! CONTENT_COMMON_DISPOSABLE_H_
