// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_WEBSOCKET_IMPL_H_
#define CONTENT_NETWORK_WEBSOCKET_IMPL_H_

#include "content/context/engine_object.h"
#include "content/public/engine_websocket.h"

namespace content {

class WebSocketImpl : public WebSocket, public EngineObject {
 public:
  WebSocketImpl(ExecutionContext* execution_context);
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
  void Send(scoped_refptr<IOStream> data,
            ExceptionState& exception_state) override;
  void Close(int32_t code,
             const std::string& reason,
             ExceptionState& exception_state) override;

 private:
  OpenHandler open_handler_;
  CloseHandler close_handler_;
  ErrorHandler error_handler_;
  MessageHandler message_handler_;

  ReadyState ready_state_;
};

}  // namespace content

#endif  //! CONTENT_NETWORK_WEBSOCKET_IMPL_H_
