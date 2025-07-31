// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "components/network/network_service_impl.h"

#include "SDL3/SDL_timer.h"

namespace network {

std::unique_ptr<NetworkService> NetworkService::LaunchService() {
  return std::unique_ptr<NetworkServiceImpl>(new NetworkServiceImpl());
}

NetworkServiceImpl::NetworkServiceImpl() {
  worker_ = base::ThreadWorker::Create();
  worker_->PostTask(base::BindOnce(&NetworkServiceImpl::StartContextInternal,
                                   base::Unretained(this)));
}

NetworkServiceImpl::~NetworkServiceImpl() {
  worker_.reset();
  context_.reset();
}

std::unique_ptr<TCPSocket> NetworkServiceImpl::CreateTCPSocket() {
  return nullptr;
}

std::unique_ptr<UDPSocket> NetworkServiceImpl::CreateUDPSocket() {
  return nullptr;
}

void NetworkServiceImpl::StartContextInternal() {
  context_ = std::make_unique<asio::io_context>(1);

  if (worker_)
    worker_->PostTask(base::BindOnce(&NetworkServiceImpl::ServiceLoopInternal,
                                     base::Unretained(this)));
}

void NetworkServiceImpl::ServiceLoopInternal() {
  if (!context_->poll())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (worker_)
    worker_->PostTask(base::BindOnce(&NetworkServiceImpl::ServiceLoopInternal,
                                     base::Unretained(this)));
}

}  // namespace  network
