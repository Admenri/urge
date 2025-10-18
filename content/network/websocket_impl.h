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

class WebSocketImpl : public WebSocket, public EngineObject {
 public:
  WebSocketImpl(ExecutionContext* execution_context, const std::string& url);
  ~WebSocketImpl() override;

  WebSocketImpl(const WebSocketImpl&) = delete;
  WebSocketImpl& operator=(const WebSocketImpl&) = delete;

 public:
  using ClientPtr = websocketpp::client<websocketpp::config::asio_client>;
  using ConnectPtr = websocketpp::connection<websocketpp::config::asio_client>;

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
  void CreateConnectInternal(const std::string& url);

  void SendPayloadInternal(std::string payload, MessageType type);
  void CloseConnectInternal(int32_t code, const std::string& reason);

  void OpenHandlerInternal(websocketpp::connection_hdl handle);
  void CloseHandlerInternal(websocketpp::connection_hdl handle);
  void ErrorHandlerInternal(websocketpp::connection_hdl handle);
  void MessageHandlerInternal(
      websocketpp::connection_hdl handle,
      websocketpp::config::asio_client::message_type::ptr message);

  std::unique_ptr<ClientPtr> client_;
  std::shared_ptr<ConnectPtr> primary_connect_;

  OpenHandler open_handler_;
  CloseHandler close_handler_;
  ErrorHandler error_handler_;
  MessageHandler message_handler_;
};

}  // namespace content

#endif  //! CONTENT_NETWORK_WEBSOCKET_IMPL_H_
