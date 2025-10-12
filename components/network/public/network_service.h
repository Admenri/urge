// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_
#define COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_

#include <memory>

#include "components/network/public/websocket_client.h"

namespace network {

class NetworkService {
 public:
  virtual ~NetworkService() = default;

  // Create global network service instance
  static std::unique_ptr<NetworkService> Create();

  // Create websocket client instance
  virtual std::unique_ptr<WebsocketClient> CreateWebsocketClient() = 0;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_
