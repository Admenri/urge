// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NET_NETWORK_CONTEXT_H_
#define CONTENT_NET_NETWORK_CONTEXT_H_

#include "base/worker/thread_worker.h"

namespace asio {
class io_context;
}

namespace content {

class NetworkContext {
 public:
  NetworkContext();
  ~NetworkContext();

  NetworkContext(const NetworkContext&) = delete;
  NetworkContext& operator=(const NetworkContext&) = delete;

  asio::io_context* GetContext() const;
  base::ThreadWorker* GetNetworkRunner() const;

 private:
  void NetworkTaskSequenceHandler();

  std::unique_ptr<asio::io_context> context_;
  std::unique_ptr<base::ThreadWorker> worker_;
};

}  // namespace content

#endif  //! CONTENT_NET_NETWORK_CONTEXT_H_
