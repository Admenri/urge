// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TCP_SOCKET_H_
#define COMPONENTS_NETWORK_TCP_SOCKET_H_

#include <memory>

#include "asio/ip/tcp.hpp"

#include "components/network/public/tcp_socket.h"

namespace network {

class TCPSocketImpl : public TCPSocket {
 public:
  TCPSocketImpl();
  ~TCPSocketImpl() override;

  TCPSocketImpl(const TCPSocketImpl&) = delete;
  TCPSocketImpl& operator=(const TCPSocketImpl&) = delete;

 private:
  std::unique_ptr<asio::ip::tcp::socket> socket_;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_TCP_SOCKET_H_
