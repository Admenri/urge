// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/websocket_impl.h"

#include "content/context/execution_context.h"

namespace content {

// static
scoped_refptr<WebSocket> WebSocket::New(ExecutionContext* execution_context,
                                        const std::string& url,
                                        ExceptionState& exception_state) {
  return base::MakeRefCounted<WebSocketImpl>(execution_context, url);
}

WebSocketImpl::WebSocketImpl(ExecutionContext* execution_context,
                             const std::string& url)
    : EngineObject(execution_context) {
  agent_ = new WebSocketAgent(execution_context, weak_ptr_factory_.GetWeakPtr(),
                              url);
}

WebSocketImpl::~WebSocketImpl() {
  context()->network_service->PostNetworkTask(
      base::BindOnce([](WebSocketAgent* agent) { delete agent; }, agent_));
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
  if (!agent_->connect_)
    return READY_STATE_CONNECTING;

  auto ready_state = agent_->connect_->get_state();
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
  if (!agent_->connect_)
    return;

  context()->network_service->PostNetworkTask(base::BindOnce(
      [](WebSocketAgent* agent, const std::string& data, MessageType type) {
        websocketpp::frame::opcode::value opcode;
        if (type == MessageType::MESSAGE_TYPE_TEXT) {
          opcode = websocketpp::frame::opcode::TEXT;
        } else {
          opcode = websocketpp::frame::opcode::BINARY;
        }

        agent->connect_->send(data, opcode);
      },
      agent_, std::move(data), type));
}

void WebSocketImpl::Close(int32_t code,
                          const std::string& reason,
                          ExceptionState& exception_state) {
  if (!agent_->connect_)
    return;

  context()->network_service->PostNetworkTask(base::BindOnce(
      [](WebSocketAgent* agent, int32_t code, const std::string& reason) {
        agent->connect_->close(code, reason);
      },
      agent_, code, reason));
}

// network thread
WebSocketAgent::WebSocketAgent(ExecutionContext* execution_context,
                               base::WeakPtr<WebSocketImpl> self,
                               const std::string& url)
    : context_(execution_context), self_(self) {
  context_->network_service->PushEvent(base::BindOnce(
      &WebSocketAgent::CreateConnectInternal, base::Unretained(this), url));
}

void WebSocketAgent::CreateConnectInternal(const std::string& url) {
  // Network service context
  auto* io_context =
      static_cast<asio::io_context*>(context_->network_service->GetIOContext());

  // Initialize client
  client_ =
      std::make_unique<websocketpp::client<websocketpp::config::asio_client>>();
  client_->clear_access_channels(websocketpp::log::alevel::all);
  client_->clear_error_channels(websocketpp::log::elevel::all);
  client_->init_asio(io_context);

  // Setup handlers
  client_->set_open_handler(
      websocketpp::lib::bind(&WebSocketAgent::OpenHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_close_handler(
      websocketpp::lib::bind(&WebSocketAgent::CloseHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_fail_handler(
      websocketpp::lib::bind(&WebSocketAgent::ErrorHandlerInternal, this,
                             websocketpp::lib::placeholders::_1));
  client_->set_message_handler(websocketpp::lib::bind(
      &WebSocketAgent::MessageHandlerInternal, this,
      websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

  // Setup connection
  websocketpp::lib::error_code ec;
  auto con = client_->get_connection(url, ec);

  // Connection
  connect_ = client_->connect(con);
}

void WebSocketAgent::OpenHandlerInternal(websocketpp::connection_hdl handle) {
  if (!self_)
    return;

  context_->network_service->PushEvent(
      base::BindRepeating(self_->open_handler_, scoped_refptr(self_.get())));
}

void WebSocketAgent::CloseHandlerInternal(websocketpp::connection_hdl handle) {
  if (!self_)
    return;

  auto ec = client_->get_con_from_hdl(handle)->get_ec();
  context_->network_service->PushEvent(
      base::BindRepeating(self_->close_handler_, scoped_refptr(self_.get()),
                          ec.value(), ec.message()));
}

void WebSocketAgent::ErrorHandlerInternal(websocketpp::connection_hdl handle) {
  if (!self_)
    return;

  auto ec = client_->get_con_from_hdl(handle)->get_ec();
  context_->network_service->PushEvent(base::BindRepeating(
      self_->error_handler_, scoped_refptr(self_.get()), ec.message()));
}

void WebSocketAgent::MessageHandlerInternal(
    websocketpp::connection_hdl handle,
    websocketpp::config::asio_client::message_type::ptr message) {
  if (!self_)
    return;

  WebSocket::MessageType opcode;
  if (message->get_opcode() == websocketpp::frame::opcode::TEXT) {
    opcode = WebSocket::MESSAGE_TYPE_TEXT;
  } else {
    opcode = WebSocket::MESSAGE_TYPE_BINARY;
  }

  auto& payload = message->get_payload();
  context_->network_service->PushEvent(base::BindRepeating(
      self_->message_handler_, scoped_refptr(self_.get()), payload, opcode));
}

}  // namespace content
