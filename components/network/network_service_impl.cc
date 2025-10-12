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

NetworkServiceImpl::NetworkServiceImpl()
    : worker_(base::ThreadWorker::Create()),
      context_(std::make_unique<asio::io_context>()) {
  // Run loop
  worker_->PostTask(base::BindOnce(&NetworkServiceImpl::MainLoopingInternal,
                                   base::Unretained(this)));
}

NetworkServiceImpl::~NetworkServiceImpl() {
  worker_.reset();
  context_.reset();
}

std::unique_ptr<WebsocketClient> NetworkServiceImpl::CreateWebsocketClient() {
  return nullptr;
}

void NetworkServiceImpl::MainLoopingInternal() {
  if (!context_->poll())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (worker_)
    worker_->PostTask(base::BindOnce(&NetworkServiceImpl::MainLoopingInternal,
                                     base::Unretained(this)));
}

}  // namespace network
