// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/websocket_impl.h"

namespace content {

// static
scoped_refptr<WebSocket> WebSocket::New(ExecutionContext* execution_context,
                                        const std::string& url,
                                        ExceptionState& exception_state) {}

WebSocketImpl::WebSocketImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context),
      ready_state_(WebSocket::READY_STATE_CONNECTING) {}

WebSocketImpl::~WebSocketImpl() {}

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
  return ready_state_;
}

void WebSocketImpl::Send(scoped_refptr<IOStream> data,
                         ExceptionState& exception_state) {}

void WebSocketImpl::Close(int32_t code,
                          const std::string& reason,
                          ExceptionState& exception_state) {}

}  // namespace content
