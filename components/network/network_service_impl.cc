// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network/network_service_impl.h"

#include "base/debug/logging.h"

namespace network {

// static
std::unique_ptr<NetworkService> NetworkService::Create() {
  return std::unique_ptr<NetworkServiceImpl>(new NetworkServiceImpl());
}

NetworkServiceImpl::NetworkServiceImpl() : quit_flag_(0) {
  // Run loop
  worker_ = std::thread(&NetworkServiceImpl::MainLoopingInternal, this);

  // Sync for initializing
  base::Semaphore semaphore;
  runner_queue_.enqueue(base::BindOnce(
      [](base::Semaphore* semaphore) { semaphore->Release(); }, &semaphore));
  semaphore.Acquire();
}

NetworkServiceImpl::~NetworkServiceImpl() {
  quit_flag_.store(1);
  worker_.join();
}

void* NetworkServiceImpl::GetIOContext() {
  return context_.get();
}

void NetworkServiceImpl::PushEvent(base::OnceClosure task) {
  if (!task.is_null())
    dispatch_queue_.enqueue(std::move(task));
}

void NetworkServiceImpl::DispatchEvent() {
  base::OnceClosure task;
  if (dispatch_queue_.try_dequeue(task) && !task.is_null()) {
    // Dispatch event from network thread
    std::move(task).Run();
  }
}

void NetworkServiceImpl::PostNetworkTask(base::OnceClosure task) {
  if (!task.is_null())
    runner_queue_.enqueue(std::move(task));
}

void NetworkServiceImpl::MainLoopingInternal() {
  context_ = std::make_unique<asio::io_context>();

  while (!quit_flag_.load()) {
    context_->poll();

    base::OnceClosure task;
    if (!runner_queue_.try_dequeue(task))
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (!task.is_null())
      std::move(task).Run();
  }

  context_.reset();
}

}  // namespace network
