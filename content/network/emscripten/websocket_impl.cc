// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/emscripten/websocket_impl.h"

#include "content/context/execution_context.h"

namespace content {

// static
scoped_refptr<WebSocket> WebSocket::New(ExecutionContext* execution_context,
                                        const std::string& url,
                                        ExceptionState& exception_state) {
  EmscriptenWebSocketCreateAttributes init_attributes;
  emscripten_websocket_init_create_attributes(&init_attributes);
  init_attributes.url = url.c_str();

  EMSCRIPTEN_WEBSOCKET_T ws_connect =
      emscripten_websocket_new(&init_attributes);
  if (!ws_connect) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to create websocket");
    return nullptr;
  }

  return base::MakeRefCounted<WebSocketImpl>(execution_context, ws_connect);
}

WebSocketImpl::WebSocketImpl(ExecutionContext* execution_context,
                             EMSCRIPTEN_WEBSOCKET_T connect)
    : EngineObject(execution_context), connect_(connect) {
  emscripten_websocket_set_onopen_callback(connect_, this,
                                           &WebSocketImpl::EMWSOpenCallback);
  emscripten_websocket_set_onclose_callback(connect_, this,
                                            &WebSocketImpl::EMWSCloseCallback);
  emscripten_websocket_set_onerror_callback(connect_, this,
                                            &WebSocketImpl::EMWSErrorCallback);
  emscripten_websocket_set_onmessage_callback(
      connect_, this, &WebSocketImpl::EMWSMessageCallback);
}

WebSocketImpl::~WebSocketImpl() {
  emscripten_websocket_delete(connect_);
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
  // RFC 6455
  uint16_t result = 0;
  emscripten_websocket_get_ready_state(connect_, &result);
  return static_cast<WebSocket::ReadyState>(result);
}

void WebSocketImpl::Send(const std::string& data,
                         MessageType type,
                         ExceptionState& exception_state) {
  if (type == MESSAGE_TYPE_TEXT) {
    emscripten_websocket_send_utf8_text(connect_, data.c_str());
  } else {
    emscripten_websocket_send_binary(connect_, const_cast<char*>(data.data()),
                                     data.size());
  }
}

void WebSocketImpl::Close(int32_t code,
                          const std::string& reason,
                          ExceptionState& exception_state) {
  emscripten_websocket_close(connect_, code, reason.c_str());
}

bool WebSocketImpl::EMWSOpenCallback(
    int eventType,
    const EmscriptenWebSocketOpenEvent* websocketEvent,
    void* userData) {
  auto* self = static_cast<WebSocketImpl*>(userData);
  self->open_handler_.Run(scoped_refptr(self));
  return true;
}

bool WebSocketImpl::EMWSCloseCallback(
    int eventType,
    const EmscriptenWebSocketCloseEvent* websocketEvent,
    void* userData) {
  auto* self = static_cast<WebSocketImpl*>(userData);
  self->close_handler_.Run(scoped_refptr(self), websocketEvent->code,
                           websocketEvent->reason);
  return true;
}

bool WebSocketImpl::EMWSErrorCallback(
    int eventType,
    const EmscriptenWebSocketErrorEvent* websocketEvent,
    void* userData) {
  auto* self = static_cast<WebSocketImpl*>(userData);
  self->error_handler_.Run(scoped_refptr(self), "emscripten internal error");
  return true;
}

bool WebSocketImpl::EMWSMessageCallback(
    int eventType,
    const EmscriptenWebSocketMessageEvent* websocketEvent,
    void* userData) {
  auto* self = static_cast<WebSocketImpl*>(userData);
  WebSocket::MessageType opcode;
  if (websocketEvent->isText) {
    opcode = MESSAGE_TYPE_TEXT;
  } else {
    opcode = MESSAGE_TYPE_BINARY;
  }

  std::string payload(websocketEvent->numBytes, 0);
  std::memcpy(payload.data(), websocketEvent->data, payload.size());
  self->message_handler_.Run(scoped_refptr(self), std::move(payload), opcode);
  return true;
}

}  // namespace content
