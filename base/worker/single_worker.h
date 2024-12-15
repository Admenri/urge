// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WORKER_SINGLE_WORKER_H_
#define BASE_WORKER_SINGLE_WORKER_H_

#include <memory>

#include "concurrentqueue/blockingconcurrentqueue.h"

#include "base/bind/callback.h"

namespace base {

class SingleWorker;

enum class WorkerScheduleMode {
  // Running on the thread with the main scheduler created.
  kCoroutine = 0,

  // Launch a new thread for current worker running.
  kAsync,
};

// Worker task traits required.
using WorkerTaskTraits = base::OnceCallback<void(SingleWorker*)>;

class SingleWorker {
 public:
  ~SingleWorker();

  SingleWorker(const SingleWorker&) = delete;
  SingleWorker& operator=(const SingleWorker&) = delete;

  static std::unique_ptr<SingleWorker> CreateWorker(WorkerScheduleMode mode);

  // Post a task closure for current worker.
  bool SendTask(WorkerTaskTraits&& task);

  // Post a semaphore flag in queue, blocking caller thread for synchronization.
  bool WaitWorkerSynchronize();

  // Flush worker running.
  void Flush();

  // Return worker scheduler mode.
  WorkerScheduleMode GetSchedulerMode() { return mode_; }

  // Used in coroutine schedule mode worker,
  // yield the context to next coroutine worker.
  void YieldFiber();

 private:
  friend class WorkerScheduler;
  friend class std::unique_ptr<SingleWorker>;
  SingleWorker(WorkerScheduleMode mode);

  WorkerScheduler* scheduler_;
  WorkerScheduleMode mode_;
  moodycamel::BlockingConcurrentQueue<WorkerTaskTraits> task_queue_;
};

}  // namespace base

#endif  // ! BASE_WORKER_SINGLE_WORKER_H_