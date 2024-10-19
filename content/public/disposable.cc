// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/disposable.h"

#include "base/exception/exception.h"

namespace content {

void Disposable::Dispose() {
  // No repeat dispose command.
  if (is_disposed_)
    return;

  // Call dispose event before set disposed flag.
  OnObjectDisposed();

  // Set disposed flag and notify observers.
  is_disposed_ = true;
  observers_.Notify();
}

base::CallbackListSubscription Disposable::AddDisposeObserver(
    base::OnceClosure observer) {
  return observers_.Add(std::move(observer));
}

void Disposable::CheckIsDisposed() const {
  if (is_disposed_) {
    throw base::Exception(base::Exception::ContentError, "Disposed object: %s",
                          DisposedObjectName().data());
  }
}

}  // namespace content
