// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "asio/io_context.hpp"

#include "content/net/network_context.h"

namespace content {

NetworkContext::NetworkContext()
    : context_(std::make_unique<asio::io_context>()),
      worker_(base::ThreadWorker::Create()) {}

NetworkContext::~NetworkContext() {
  worker_.reset();
  context_.reset();
}

asio::io_context* NetworkContext::GetContext() const {
  return context_.get();
}

base::ThreadWorker* NetworkContext::GetNetworkRunner() const {
  return worker_.get();
}

void NetworkContext::NetworkTaskSequenceHandler() {
  if (!context_->poll())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (worker_)
    worker_->PostTask(base::BindOnce(
        &NetworkContext::NetworkTaskSequenceHandler, base::Unretained(this)));
}

}  // namespace content
