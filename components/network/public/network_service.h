// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_
#define COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_

#include <memory>

#include "base/bind/callback.h"
#include "base/worker/thread_worker.h"

namespace network {

class NetworkService {
 public:
  virtual ~NetworkService() = default;

  // Create global network service instance
  static std::unique_ptr<NetworkService> Create();

  // ASIO Context
  virtual void* GetIOContext() = 0;

  // Push back a task on dispatching queue
  virtual void PushEvent(base::OnceClosure task) = 0;

  // Dispatch async event on current thread
  virtual void DispatchEvent() = 0;

  // Network thread
  virtual void PostNetworkTask(base::OnceClosure task) = 0;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_
