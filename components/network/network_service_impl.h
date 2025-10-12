// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_
#define COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_

#include <memory>
#include <thread>

#include "asio.hpp"

#include "base/worker/thread_worker.h"
#include "components/network/public/network_service.h"

namespace network {

class NetworkServiceImpl : public NetworkService {
 public:
  NetworkServiceImpl();
  ~NetworkServiceImpl() override;

  NetworkServiceImpl(const NetworkServiceImpl&) = delete;
  NetworkServiceImpl& operator=(const NetworkServiceImpl&) = delete;

 protected:
  std::unique_ptr<WebsocketClient> CreateWebsocketClient() override;

 private:
  void MainLoopingInternal();

  std::unique_ptr<base::ThreadWorker> worker_;
  std::unique_ptr<asio::io_context> context_;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_
