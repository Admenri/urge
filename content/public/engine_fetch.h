// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_FETCH_H_
#define CONTENT_PUBLIC_ENGINE_FETCH_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_iostream.h"

namespace content {

/*--urge(name:HTTPRequest)--*/
struct URGE_OBJECT(HTTPRequest) {
  std::string url;
  std::string headers;
  std::string method;
  scoped_refptr<IOStream> body;
};

/*--urge(name:HTTPResponse)--*/
struct URGE_OBJECT(HTTPResponse) {
  std::string url;
  std::string headers;
  uint32_t status;
  std::string status_text;
  scoped_refptr<IOStream> body;
};

/*--urge(name:Fetch)--*/
class URGE_OBJECT(Fetch) {
 public:
  virtual ~Fetch() = default;

  /*--urge(name:ReadyState)--*/
  enum ReadyState {
    STATE_UNSENT = 0,
    STATE_LOADING,
    STATE_DONE,
  };

  /*--urge(name:ResponseHandler)--*/
  using ResponseHandler =
      base::RepeatingCallback<void(scoped_refptr<HTTPResponse> response)>;

  /*--urge(name:ProgressHandler)--*/
  using ProgressHandler =
      base::RepeatingCallback<void(uint64_t current, uint64_t total)>;

  /*--urge(name:initialize)--*/
  static scoped_refptr<Fetch> New(ExecutionContext* execution_context,
                                  scoped_refptr<HTTPRequest> request,
                                  ExceptionState& exception_state);

  /*--urge(name:send)--*/
  virtual void Send(ExceptionState& exception_state) = 0;

  /*--urge(name:abort)--*/
  virtual void Abort(ExceptionState& exception_state) = 0;

  /*--urge(name:ready_state)--*/
  virtual ReadyState GetReadyState(ExceptionState& exception_state) = 0;

  /*--urge(name:set_response_handler)--*/
  virtual void SetResponseHandler(ResponseHandler handler,
                                  ExceptionState& exception_state) = 0;

  /*--urge(name:set_progress_handler)--*/
  virtual void SetProgressHandler(ProgressHandler handler,
                                  ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_FETCH_H_
