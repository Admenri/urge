// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_
#define CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:HTTPRequest)--*/
class URGE_OBJECT(HTTPRequest) {
 public:
  virtual ~HTTPRequest() = default;

  /*--urge(name:ReadyState)--*/
  enum ReadyState {
    STATE_UNSENT = 0,
    STATE_OPENED = 1,
    STATE_HEADERS_RECEIVED = 2,
    STATE_LOADING = 3,
    STATE_DONE = 4,
  };

  /*--urge(name:ProgressHandler)--*/
  using ProgressHandler =
      base::RepeatingCallback<void(uint64_t current, uint64_t total)>;

  /*--urge(name:initialize)--*/
  static scoped_refptr<HTTPRequest> New(ExecutionContext* execution_context,
                                        ExceptionState& exception_state);

  /*--urge(name:open,optional:username="",optional:password="")--*/
  virtual void Open(const std::string& method,
                    const std::string& url,
                    const std::string& mime_type,
                    const std::string& username,
                    const std::string& password,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:send,optional:body="")--*/
  virtual void Send(const std::string& body,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:abort)--*/
  virtual void Abort(ExceptionState& exception_state) = 0;

  /*--urge(name:set_timeout)--*/
  virtual void SetTimeout(uint32_t timeout,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:set_request_header)--*/
  virtual void SetRequestHeader(const std::string& key,
                                const std::string& value,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:response)--*/
  virtual std::string GetResponse(ExceptionState& exception_state) = 0;

  /*--urge(name:response_header)--*/
  virtual std::string GetResponseHeader(const std::string& header,
                                        ExceptionState& exception_state) = 0;

  /*--urge(name:ready_state)--*/
  virtual ReadyState GetReadyState(ExceptionState& exception_state) = 0;

  /*--urge(name:status)--*/
  virtual uint32_t GetStatus(ExceptionState& exception_state) = 0;

  /*--urge(name:url)--*/
  virtual std::string GetURL(ExceptionState& exception_state) = 0;

  /*--urge(name:set_abort_handler)--*/
  virtual void SetAbortHandler(ProgressHandler handler,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:set_error_handler)--*/
  virtual void SetErrorHandler(ProgressHandler handler,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:set_load_handler)--*/
  virtual void SetLoadHandler(ProgressHandler handler,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_load_end_handler)--*/
  virtual void SetLoadEndHandler(ProgressHandler handler,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:set_load_start_handler)--*/
  virtual void SetLoadStartHandler(ProgressHandler handler,
                                   ExceptionState& exception_state) = 0;

  /*--urge(name:set_progress_handler)--*/
  virtual void SetProgressHandler(ProgressHandler handler,
                                  ExceptionState& exception_state) = 0;

  /*--urge(name:set_ready_state_change_handler)--*/
  virtual void SetReadyStateChangeHandler(ProgressHandler handler,
                                          ExceptionState& exception_state) = 0;

  /*--urge(name:set_timeout_handler)--*/
  virtual void SetTimeoutHandler(ProgressHandler handler,
                                 ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_
