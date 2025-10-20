// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/websocket_impl.h"

#include "content/context/execution_context.h"

namespace content {

namespace {

void DestroyConnectInternal(
    std::unique_ptr<WebSocketImpl::ClientPtr> client,
    std::shared_ptr<WebSocketImpl::ConnectPtr> connect) {
  connect.reset();
  client.reset();
}

}  // namespace

// static
scoped_refptr<WebSocket> WebSocket::New(ExecutionContext* execution_context,
                                        const std::string& url,
                                        ExceptionState& exception_state) {
  return base::MakeRefCounted<WebSocketImpl>(execution_context, url);
}

WebSocketImpl::WebSocketImpl(ExecutionContext* execution_context,
                             const std::string& url)
    : EngineObject(execution_context) {
  context()->network_service->PostNetworkTask(base::BindOnce(
      &WebSocketImpl::CreateConnectInternal, base::Unretained(this), url));
}

WebSocketImpl::~WebSocketImpl() {
  context()->network_service->PostNetworkTask(
      base::BindOnce(&DestroyConnectInternal, std::move(client_),
                     std::move(primary_connect_)));
}

void WebSocketImpl::SetOpenHandler(OpenHandler callback,
                                   ExceptionState& exception_state) {
  open_handler_ = callback;
}

void WebSocketImpl::SetCloseHandler(CloseHandler callback,
                                    ExceptionState& exception_state) {
  close_handler_ = callback;
}

void WebSocketImpl::SetErrorHandler(ErrorHandler callback,
                                    ExceptionState& exception_state) {
  error_handler_ = callback;
}

void WebSocketImpl::SetMessageHandler(MessageHandler callback,
                                      ExceptionState& exception_state) {
  message_handler_ = callback;
}

WebSocket::ReadyState WebSocketImpl::GetReadyState(
    ExceptionState& exception_state) {
  if (!primary_connect_)
    return READY_STATE_CONNECTING;

  auto ready_state = primary_connect_->get_state();
  switch (ready_state) {
    default:
    case websocketpp::session::state::value::connecting:
      return READY_STATE_CONNECTING;
    case websocketpp::session::state::value::open:
      return READY_STATE_OPEN;
    case websocketpp::session::state::value::closing:
      return READY_STATE_CLOSING;
    case websocketpp::session::state::value::closed:
      return READY_STATE_CLOSE;
  }
}

void WebSocketImpl::Send(const std::string& data,
                         MessageType type,
                         ExceptionState& exception_state) {
  context()->network_service->PostNetworkTask(
      base::BindOnce(&WebSocketImpl::SendPayloadInternal,
                     base::Unretained(this), std::move(data), type));
}

void WebSocketImpl::Close(int32_t code,
                          const std::string& reason,
                          ExceptionState& exception_state) {
  context()->network_service->PostNetworkTask(
      base::BindOnce(&WebSocketImpl::CloseConnectInternal,
                     base::Unretained(this), code, reason));
}

// network thread
void WebSocketImpl::CreateConnectInternal(const std::string& url) {
  // Network service context
  auto* io_context = static_cast<asio::io_context*>(
      context()->network_service->GetIOContext());

  // Initialize client
  client_ =
      std::make_unique<websocketpp::client<websocketpp::config::asio_client>>();
  client_->clear_access_channels(websocketpp::log::alevel::all);
  client_->clear_error_channels(websocketpp::log::elevel::all);
  client_->init_asio(io_context);
  client_->start_perpetual();

  // Setup handlers
  client_->set_open_handler(
      websocketpp::lib::bind(&WebSocketImpl::OpenHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_close_handler(
      websocketpp::lib::bind(&WebSocketImpl::CloseHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_fail_handler(
      websocketpp::lib::bind(&WebSocketImpl::ErrorHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_message_handler(websocketpp::lib::bind(
      &WebSocketImpl::MessageHandlerInternal, this,
      websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

  // Setup connection
  websocketpp::lib::error_code ec;
  auto con = client_->get_connection(url, ec);

  // Connection
  primary_connect_ = client_->connect(con);
}

void WebSocketImpl::SendPayloadInternal(std::string payload, MessageType type) {
  websocketpp::frame::opcode::value opcode;
  if (type == MessageType::MESSAGE_TYPE_TEXT) {
    opcode = websocketpp::frame::opcode::TEXT;
  } else {
    opcode = websocketpp::frame::opcode::BINARY;
  }

  primary_connect_->send(std::move(payload), opcode);
}

void WebSocketImpl::CloseConnectInternal(int32_t code,
                                         const std::string& reason) {}

void WebSocketImpl::OpenHandlerInternal(websocketpp::connection_hdl handle) {
  context()->network_service->PushEvent(
      base::BindRepeating(open_handler_, scoped_refptr(this)));
}

void WebSocketImpl::CloseHandlerInternal(websocketpp::connection_hdl handle) {
  auto ec = client_->get_con_from_hdl(handle)->get_ec();
  context()->network_service->PushEvent(base::BindRepeating(
      close_handler_, scoped_refptr(this), ec.value(), ec.message()));
}

void WebSocketImpl::ErrorHandlerInternal(websocketpp::connection_hdl handle) {
  auto ec = client_->get_con_from_hdl(handle)->get_ec();
  context()->network_service->PushEvent(
      base::BindRepeating(error_handler_, scoped_refptr(this), ec.message()));
}

void WebSocketImpl::MessageHandlerInternal(
    websocketpp::connection_hdl handle,
    websocketpp::config::asio_client::message_type::ptr message) {
  MessageType opcode;
  if (message->get_opcode() == websocketpp::frame::opcode::TEXT) {
    opcode = MessageType::MESSAGE_TYPE_TEXT;
  } else {
    opcode = MessageType::MESSAGE_TYPE_BINARY;
  }

  auto& payload = message->get_payload();
  context()->network_service->PushEvent(base::BindRepeating(
      message_handler_, scoped_refptr(this), payload, opcode));
}

}  // namespace content
