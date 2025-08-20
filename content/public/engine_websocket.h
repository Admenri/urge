// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_WEBSOCKET_H_
#define CONTENT_PUBLIC_ENGINE_WEBSOCKET_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:WebSocket)--*/
class URGE_OBJECT(WebSocket) {
 public:
  virtual ~WebSocket() = default;

  /*--urge(name:ReadyState)--*/
  enum ReadyState {
    STATE_CONNECTING = 0,
    STATE_OPEN = 1,
    STATE_CLOSING = 2,
    STATE_CLOSED = 3,
  };

  /*--urge(name:MessageType)--*/
  enum MessageType {
    MESSAGE_TYPE_TEXT = 0,
    MESSAGE_TYPE_BINARY,
  };

  /*--urge(name:OpenHandler)--*/
  using OpenHandler = base::RepeatingCallback<void()>;

  /*--urge(name:ErrorHandler)--*/
  using ErrorHandler = base::RepeatingCallback<void()>;

  /*--urge(name:MessageHandler)--*/
  using MessageHandler =
      base::RepeatingCallback<void(const std::string& message,
                                   MessageType type)>;

  /*--urge(name:CloseHandler)--*/
  using CloseHandler =
      base::RepeatingCallback<void(int32_t code, const std::string& reason)>;

  /*--urge(name:initialize)--*/
  static scoped_refptr<WebSocket> New(ExecutionContext* execution_context,
                                      ExceptionState& exception_state);

  /*--urge(name:connect)--*/
  virtual void Connect(const std::string& url,
                       const std::vector<std::string>& protocols,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:close)--*/
  virtual void Close(int32_t code,
                     const std::string& reason,
                     ExceptionState& exception_state) = 0;

  /*--urge(name:send)--*/
  virtual void Send(const std::string& message,
                    MessageType type,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:ready_state)--*/
  virtual ReadyState GetReadyState(ExceptionState& exception_state) = 0;

  /*--urge(name:protocol)--*/
  virtual std::string GetProtocol(ExceptionState& exception_state) = 0;

  /*--urge(name:url)--*/
  virtual std::string GetURL(ExceptionState& exception_state) = 0;

  /*--urge(name:buffered_amount)--*/
  virtual uint32_t GetBufferedAmount(ExceptionState& exception_state) = 0;

  /*--urge(name:set_open_handler)--*/
  virtual void SetOpenHandler(OpenHandler handler,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_error_handler)--*/
  virtual void SetErrorHandler(ErrorHandler handler,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:set_message_handler)--*/
  virtual void SetMessageHandler(MessageHandler handler,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:set_close_handler)--*/
  virtual void SetCloseHandler(CloseHandler handler,
                               ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_WEBSOCKET_H_
