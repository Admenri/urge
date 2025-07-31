// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_
#define COMPONENTS_NETWORK_PUBLIC_NETWORK_SERVICE_H_

#include <memory>

#include "components/network/public/tcp_socket.h"
#include "components/network/public/udp_socket.h"

namespace network {

class NetworkService {
 public:
  virtual ~NetworkService() = default;

  static std::unique_ptr<NetworkService> LaunchService();

  virtual std::unique_ptr<TCPSocket> CreateTCPSocket() = 0;

  virtual std::unique_ptr<UDPSocket> CreateUDPSocket() = 0;
};

}  // namespace  network

#endif  //! COMPONENTS_NETWORK_NETWORK_SERVICE_H_
