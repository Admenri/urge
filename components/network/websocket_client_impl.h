// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_WEBSOCKET_CLIENT_IMPL_H_
#define COMPONENTS_NETWORK_WEBSOCKET_CLIENT_IMPL_H_

#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_client.hpp"

#include "components/network/public/websocket_client.h"

namespace network {

class WebsocketClientImpl : public WebsocketClient {
 public:
  WebsocketClientImpl();
  ~WebsocketClientImpl() override;

  WebsocketClientImpl(const WebsocketClientImpl&) = delete;
  WebsocketClientImpl& operator=(const WebsocketClientImpl&) = delete;

 private:
  websocketpp::client<websocketpp::config::asio_client> client_;
};

}  // namespace network

#endif  //! COMPONENTS_NETWORK_WEBSOCKET_CLIENT_IMPL_H_
