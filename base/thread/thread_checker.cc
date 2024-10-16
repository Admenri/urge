// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread/thread_checker.h"

namespace base {
namespace internal {

ThreadCheckerImpl::ThreadCheckerImpl() : valid_thread_id_() {
  EnsureThreadIdAssigned();
}

ThreadCheckerImpl::~ThreadCheckerImpl() {}

bool ThreadCheckerImpl::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  AutoLock auto_lock(lock_);
  return valid_thread_id_ == std::this_thread::get_id();
}

void ThreadCheckerImpl::DetachFromThread() {
  AutoLock auto_lock(lock_);
  valid_thread_id_ = std::this_thread::get_id();
}

void ThreadCheckerImpl::EnsureThreadIdAssigned() const {
  AutoLock auto_lock(lock_);
  if (valid_thread_id_ == std::thread::id()) {
    valid_thread_id_ = std::this_thread::get_id();
  }
}

}  // namespace internal
}  // namespace base
