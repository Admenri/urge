// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/context/disposable.h"

namespace content {

Disposable::Disposable(DisposableCollection* parent) : disposed_(0) {
  if (parent)
    parent->AddDisposable(this);
}

Disposable::~Disposable() {
  base::LinkNode<Disposable>::RemoveFromList();
}

void Disposable::Dispose() {
  if (!disposed_) {
    OnObjectDisposed();
    disposed_ = true;
  }
}

bool Disposable::CheckDisposed(ExceptionState& exception_state) {
  if (disposed_) {
    // Throw exception if need
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Disposed object: %s",
                               DisposedObjectName().c_str());
  }

  return disposed_;
}

}  // namespace content
