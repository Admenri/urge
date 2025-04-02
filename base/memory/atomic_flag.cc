// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_ATOMIC_FLAG_H_
#define BASE_MEMORY_ATOMIC_FLAG_H_

#include <atomic>

#include "base/debug/logging.h"
#include "base/threading/thread_checker.h" // Include for ThreadChecker

namespace base {

// A flag that can be safely set on one thread and checked on multiple threads.
// Provides memory synchronization similar to a WaitableEvent, but without the
// OS overhead.
//
// IMPORTANT: The owner of the AtomicFlag must ensure that Set() is always called
// from the same sequence (thread). IsSet() can be called from any thread.
// The thread/sequence constraint is checked via a ThreadChecker.
//
// Typical Use:
//
//   // Initialization (on any thread)
//   AtomicFlag my_flag;
//
//   // --- On the setting sequence/thread ---
//   ... do some work ...
//   my_flag.Set(); // Signal completion, synchronize memory.
//
//   // --- On a checking thread ---
//   if (my_flag.IsSet()) {
//     // Can safely access memory written before my_flag.Set()
//     // on the setting thread.
//   }
//
// Alternatively, use TrySet() if only the first thread to attempt setting
// should succeed:
//
//   // --- On potentially multiple threads ---
//   if (my_flag.TrySet()) {
//     // This thread was the first to set the flag.
//     // Perform the one-time initialization/action.
//   }
//
class AtomicFlag {
 public:
  AtomicFlag();
  ~AtomicFlag();

  AtomicFlag(const AtomicFlag&) = delete;
  AtomicFlag& operator=(const AtomicFlag&) = delete;

  // Sets the flag.
  // Must always be called from the same sequence. Provides release semantics,
  // ensuring that prior memory writes become visible to threads calling
  // IsSet() or TrySet() (that observe the flag as set) afterwards.
  void Set();

  // Returns true if the flag has been set.
  // Provides acquire semantics, ensuring that if it returns true, subsequent
  // memory reads can safely access memory written before the corresponding
  // Set() or TrySet() call. Can be called from any thread.
  bool IsSet() const;

  // Attempts to set the flag. Returns true if the flag was successfully set
  // (i.e., it was previously unset), false otherwise.
  // Must always be called from the same sequence as Set().
  // Provides acquire/release semantics. If it returns true, it acts like Set().
  // If it returns false, it still synchronizes with the operation that
  // originally set the flag.
  bool TrySet();

  // Resets the flag to its initial state (unset) and allows Set/TrySet to be
  // called from a potentially different sequence afterwards.
  // *** THIS IS UNSAFE and breaks the typical guarantee. ***
  // *** ONLY use this in test scenarios where you understand the risks. ***
  // It provides no memory synchronization guarantees for other threads
  // observing this reset.
  void UnsafeResetForTesting();

 private:
  // Thread checker to ensure Set() and TrySet() are called from the same sequence.
  ThreadChecker set_thread_checker_;

  // The atomic flag value. 0 = unset, 1 = set.
  std::atomic<int> flag_{0};
};

// --- Implementation ---

inline AtomicFlag::AtomicFlag() {
  // It doesn't matter where the AtomicFlag is constructed so long as Set()
  // and TrySet() are always called from the same sequence thereafter.
  // Detach the checker here so the constructor thread doesn't bind it.
  set_thread_checker_.DetachFromThread();
}

inline AtomicFlag::~AtomicFlag() = default;

inline void AtomicFlag::Set() {
  DCHECK(set_thread_checker_.CalledOnValidThread());
  // Use release semantics so that all memory writes preceding this store
  // are visible to threads that load the value using acquire semantics
  // and see it as set (1).
  flag_.store(1, std::memory_order_release);
}

inline bool AtomicFlag::IsSet() const {
  // Use acquire semantics so that if this load sees the value 1, we are
  // guaranteed to see all memory writes that happened before the
  // corresponding Set()/TrySet() call (which used release semantics).
  return flag_.load(std::memory_order_acquire) != 0;
}

inline bool AtomicFlag::TrySet() {
  DCHECK(set_thread_checker_.CalledOnValidThread());
  // Use memory_order_acq_rel for the exchange.
  // - Release semantics are needed like in Set() if we successfully change 0->1.
  // - Acquire semantics are needed like in IsSet() if we read the value (either
  //   the old 0 or the existing 1) to synchronize with the operation that set it.
  // `exchange` atomically sets the value to 1 and returns the *previous* value.
  int old_value = flag_.exchange(1, std::memory_order_acq_rel);
  // Return true only if we were the ones who changed it from 0 to 1.
  return old_value == 0;
}


inline void AtomicFlag::UnsafeResetForTesting() {
  // Release semantics are used here primarily for consistency, though the
  // "unsafe" nature means callers shouldn't rely on strict synchronization
  // around this specific call outside of controlled test environments.
  flag_.store(0, std::memory_order_release);
  // Allow Set/TrySet to be called from a different thread in the future.
  set_thread_checker_.DetachFromThread();
}

}  // namespace base

#endif // BASE_MEMORY_ATOMIC_FLAG_H_
