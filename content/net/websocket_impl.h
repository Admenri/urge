// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NET_WEBSOCKET_IMPL_H_
#define CONTENT_NET_WEBSOCKET_IMPL_H_

#include "content/context/engine_object.h"
#include "content/public/engine_websocket.h"

namespace content {

class WebSocketImpl : public WebSocket, public EngineObject {
 public:
  WebSocketImpl();
  ~WebSocketImpl() override;

  WebSocketImpl(const WebSocketImpl&) = delete;
  WebSocketImpl& operator=(const WebSocketImpl&) = delete;

  void Connect(const std::string& url,
               const std::vector<std::string>& protocols,
               ExceptionState& exception_state) override;
  void Close(int32_t code,
             const std::string& reason,
             ExceptionState& exception_state) override;
  void Send(const std::string& message,
            MessageType type,
            ExceptionState& exception_state) override;
  ReadyState GetReadyState(ExceptionState& exception_state) override;
  std::string GetProtocol(ExceptionState& exception_state) override;
  std::string GetURL(ExceptionState& exception_state) override;
  uint32_t GetBufferedAmount(ExceptionState& exception_state) override;
  void SetOpenHandler(OpenHandler handler,
                      ExceptionState& exception_state) override;
  void SetErrorHandler(ErrorHandler handler,
                       ExceptionState& exception_state) override;
  void SetMessageHandler(MessageHandler handler,
                         ExceptionState& exception_state) override;
  void SetCloseHandler(CloseHandler handler,
                       ExceptionState& exception_state) override;

 private:
};

}  // namespace  content

#endif  //! CONTENT_NET_WEBSOCKET_IMPL_H_
