// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_
#define COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_

#include <memory>
#include <thread>

#include "asio.hpp"
#include "concurrentqueue/concurrentqueue.h"

#include "components/network/public/network_service.h"

namespace network {

class NetworkServiceImpl : public NetworkService {
 public:
  NetworkServiceImpl();
  ~NetworkServiceImpl() override;

  NetworkServiceImpl(const NetworkServiceImpl&) = delete;
  NetworkServiceImpl& operator=(const NetworkServiceImpl&) = delete;

 protected:
  void* GetIOContext() override;
  void PushEvent(base::OnceClosure task) override;
  void DispatchEvent() override;
  void PostNetworkTask(base::OnceClosure task) override;

 private:
  void MainLoopingInternal();

  std::thread worker_;
  std::atomic<int32_t> quit_flag_;
  std::unique_ptr<asio::io_context> context_;

  moodycamel::ConcurrentQueue<base::OnceClosure> runner_queue_;
  moodycamel::ConcurrentQueue<base::OnceClosure> dispatch_queue_;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_NETWORK_SERVICE_IMPL_H_
