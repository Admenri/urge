// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_WEBSOCKET_IMPL_H_
#define CONTENT_NETWORK_WEBSOCKET_IMPL_H_

#include <memory>

#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_no_tls_client.hpp"

#include "content/context/engine_object.h"
#include "content/public/engine_websocket.h"

namespace content {

struct WebSocketAgent;

class WebSocketImpl : public WebSocket, public EngineObject {
 public:
  WebSocketImpl(ExecutionContext* execution_context, const std::string& url);
  ~WebSocketImpl() override;

  WebSocketImpl(const WebSocketImpl&) = delete;
  WebSocketImpl& operator=(const WebSocketImpl&) = delete;

 public:
  void SetOpenHandler(OpenHandler callback,
                      ExceptionState& exception_state) override;
  void SetCloseHandler(CloseHandler callback,
                       ExceptionState& exception_state) override;
  void SetErrorHandler(ErrorHandler callback,
                       ExceptionState& exception_state) override;
  void SetMessageHandler(MessageHandler callback,
                         ExceptionState& exception_state) override;
  ReadyState GetReadyState(ExceptionState& exception_state) override;
  void Send(const std::string& data,
            MessageType type,
            ExceptionState& exception_state) override;
  void Close(int32_t code,
             const std::string& reason,
             ExceptionState& exception_state) override;

 private:
  friend struct WebSocketAgent;
  void CreateConnectInternal(const std::string& url);

  WebSocketAgent* agent_;

  OpenHandler open_handler_;
  CloseHandler close_handler_;
  ErrorHandler error_handler_;
  MessageHandler message_handler_;

  base::WeakPtrFactory<WebSocketImpl> weak_ptr_factory_{this};
};

struct WebSocketAgent {
  using ClientPtr = websocketpp::client<websocketpp::config::asio_client>;
  using ConnectPtr = websocketpp::connection<websocketpp::config::asio_client>;

  ExecutionContext* context_;
  base::WeakPtr<WebSocketImpl> self_;
  std::unique_ptr<ClientPtr> client_;
  std::shared_ptr<ConnectPtr> connect_;

  WebSocketAgent(ExecutionContext* execution_context,
                 base::WeakPtr<WebSocketImpl> self,
                 const std::string& url);

  void CreateConnectInternal(const std::string& url);

  void OpenHandlerInternal(websocketpp::connection_hdl handle);
  void CloseHandlerInternal(websocketpp::connection_hdl handle);
  void ErrorHandlerInternal(websocketpp::connection_hdl handle);
  void MessageHandlerInternal(
      websocketpp::connection_hdl handle,
      websocketpp::config::asio_client::message_type::ptr message);
};

}  // namespace content

#endif  //! CONTENT_NETWORK_WEBSOCKET_IMPL_H_
