// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker/thread_worker.h"

#include "concurrentqueue/blockingconcurrentqueue.h"

namespace base {

struct ThreadWorker::QueueInternal {
  moodycamel::ConcurrentQueue<OnceClosure> queue;

  auto* operator->() { return &queue; }
};

ThreadWorker::ThreadWorker()
    : quit_flag_(0), task_queue_(std::make_unique<QueueInternal>()) {}

ThreadWorker::~ThreadWorker() {
  quit_flag_.store(1);
  thread_.join();
}

std::unique_ptr<ThreadWorker> ThreadWorker::Create() {
  std::unique_ptr<ThreadWorker> instance(new ThreadWorker);

  std::thread thread(&ThreadWorker::ThreadMainFunctionInternal, instance.get());
  instance->thread_ = std::move(thread);

  return instance;
}

bool ThreadWorker::PostTask(OnceClosure task) {
  return (*task_queue_)->enqueue(std::move(task));
}

void ThreadWorker::WaitWorkerSynchronize() {
  Semaphore semaphore;
  OnceClosure required_task = base::BindOnce(
      [](Semaphore* semaphore) { semaphore->Release(); }, &semaphore);
  (*task_queue_)->enqueue(std::move(required_task));
  semaphore.Acquire();
}

bool ThreadWorker::RunsTasksInCurrentSequence() {
  return thread_.get_id() == std::this_thread::get_id();
}

bool ThreadWorker::DeleteOrReleaseSoonInternal(void (*deleter)(const void*),
                                               const void* object) {
  PostTask(base::BindOnce(deleter, object));
  return !!object;
}

void ThreadWorker::ThreadMainFunctionInternal() {
  while (!quit_flag_) {
    OnceClosure queued_task;

    if ((*task_queue_)->try_dequeue(queued_task)) {
      std::move(queued_task).Run();
      continue;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}

}  // namespace base
