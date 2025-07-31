// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_UDP_SOCKET_H_
#define COMPONENTS_NETWORK_UDP_SOCKET_H_

#include <memory>

#include "asio/ip/udp.hpp"

#include "components/network/public/udp_socket.h"

namespace network {

class UDPSocketImpl : public UDPSocket {
 public:
  UDPSocketImpl();
  ~UDPSocketImpl() override;

  UDPSocketImpl(const UDPSocketImpl&) = delete;
  UDPSocketImpl& operator=(const UDPSocketImpl&) = delete;

 private:
  std::unique_ptr<asio::ip::udp::socket> socket_;
};

}  // namespace  network

#endif  //! COMPONENTS_NETWORK_UDP_SOCKET_H_
