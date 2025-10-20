// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_
#define CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_iostream.h"

namespace content {

/*--urge(name:HTTPRequest)--*/
class URGE_OBJECT(HTTPRequest) {
 public:
  virtual ~HTTPRequest() = default;

  /*--urge(name:ReadyState)--*/
  enum ReadyState {
    READY_STATE_UNSENT = 0,
    READY_STATE_OPENED,
    READY_STATE_HEADERS_RECEIVED,
    READY_STATE_LOADING,
    READY_STATE_DONE,
  };

  /*--urge(name:ReadyStateHandler)--*/
  using ReadyStateHandler =
      base::RepeatingCallback<void(scoped_refptr<HTTPRequest> itself)>;

  /*--urge(name:initialize)--*/
  static scoped_refptr<HTTPRequest> New(ExecutionContext* execution_context,
                                        ExceptionState& exception_state);

  /*--urge(name:set_ready_state_handler)--*/
  virtual void SetReadyStateHandler(ReadyStateHandler callback,
                                    ExceptionState& exception_state) = 0;

  /*--urge(name:set_request_header)--*/
  virtual void SetRequestHeader(const std::string& key,
                                const std::string& value,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:ready_state)--*/
  virtual ReadyState GetReadyState(ExceptionState& exception_state) = 0;

  /*--urge(name:status_code)--*/
  virtual int32_t GetStatusCode(ExceptionState& exception_state) = 0;

  /*--urge(name:status_text)--*/
  virtual std::string GetStatusText(ExceptionState& exception_state) = 0;

  /*--urge(name:response_headers)--*/
  virtual std::string GetResponseHeaders(ExceptionState& exception_state) = 0;

  /*--urge(name:response)--*/
  virtual scoped_refptr<IOStream> GetResponse(
      ExceptionState& exception_state) = 0;

  /*--urge(name:open)--*/
  virtual void Open(const std::string& method,
                    const std::string& url,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:send,optional:body=nullptr)--*/
  virtual void Send(scoped_refptr<IOStream> body,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:abort)--*/
  virtual void Abort(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_
