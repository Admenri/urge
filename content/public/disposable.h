// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_DISPOSABLE_H_
#define CONTENT_PUBLIC_DISPOSABLE_H_

#include "base/bind/callback_list.h"
#include "base/buildflags/build.h"
#include "base/containers/linked_list.h"
#include "content/common/content_utils.h"

namespace content {

class Disposable;

// Collection, disposed all pooling object when finally.
// There's the parent container class defination.
class DisposableCollection {
 public:
  virtual void AddDisposable(Disposable* child) = 0;
  virtual void RemoveDisposable(Disposable* child) = 0;
};

// Collection, disposed all pooling object when finally.
// Child defination.
class Disposable {
 public:
  Disposable(DisposableCollection* collection)
      : collection_(collection), link_(this), is_disposed_(false) {
    if (collection_)
      collection_->AddDisposable(this);
  }

  virtual ~Disposable() {
    if (collection_)
      collection_->RemoveDisposable(this);
  }

  Disposable(const Disposable&) = delete;
  Disposable& operator=(const Disposable&) = delete;

 public:
  CONTENT_EXPORT bool IsDisposed() const { return is_disposed_; }
  CONTENT_EXPORT void Dispose();

 public:
  base::CallbackListSubscription AddDisposeObserver(base::OnceClosure observer);
  void CheckIsDisposed() const;
  base::LinkNode<Disposable>* disposable_link() { return &link_; }

 protected:
  virtual void OnObjectDisposed() = 0;
  virtual std::string_view DisposedObjectName() const = 0;

 private:
  base::OnceClosureList observers_;
  DisposableCollection* collection_;
  base::LinkNode<Disposable> link_;
  bool is_disposed_;
};

inline static bool IsObjectValid(Disposable* target) {
  return target && !target->IsDisposed();
}

}  // namespace content

#endif  // !CONTENT_PUBLIC_DISPOSABLE_H_