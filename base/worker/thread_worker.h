// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WORKER_THREAD_WORKER_H_
#define BASE_WORKER_THREAD_WORKER_H_

#include <condition_variable>
#include <memory>
#include <mutex>

#include "base/bind/callback.h"

namespace base {

class Semaphore {
 public:
  explicit Semaphore() : mutex_(), cv_(), count_(0) {}

  void Acquire() {
    std::unique_lock lock(mutex_);
    ++count_;
    cv_.wait(lock, [this] { return count_ == 0; });
  }

  void Release() {
    std::scoped_lock lock(mutex_);
    --count_;
    cv_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  int32_t count_;
};

class ThreadWorker {
 public:
  struct QueueInternal;

  ~ThreadWorker();

  ThreadWorker(const ThreadWorker&) = delete;
  ThreadWorker& operator=(const ThreadWorker&) = delete;

  static base::OwnedPtr<ThreadWorker> Create();

  // Post a task closure for current worker.
  bool PostTask(OnceClosure task);

  // Post a semaphore flag in queue, blocking caller thread for synchronization.
  void WaitWorkerSynchronize();

  // Is current context running on current single worker?
  bool RunsTasksInCurrentSequence();

  // Inline static version
  static bool PostTask(ThreadWorker* worker, OnceClosure task) {
    if (worker)
      return worker->PostTask(std::move(task));
    std::move(task).Run();
    return true;
  }

  static void WaitWorkerSynchronize(ThreadWorker* worker) {
    if (worker)
      return worker->WaitWorkerSynchronize();
  }

  static bool RunsTasksInCurrentSequence(ThreadWorker* worker) {
    if (worker)
      return worker->RunsTasksInCurrentSequence();
    return true;
  }

 private:
  friend struct base::Allocator;
  ThreadWorker();
  void ThreadMainFunctionInternal();
  bool DeleteOrReleaseSoonInternal(void (*deleter)(const void*),
                                   const void* object);

  std::thread thread_;
  std::atomic<int32_t> quit_flag_;
  base::OwnedPtr<QueueInternal> task_queue_;
};

}  // namespace base

#endif  //! BASE_WORKER_THREAD_WORKER_H_
