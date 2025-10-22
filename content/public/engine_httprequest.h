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

/*--urge(name:HTTPRequestOptions)--*/
struct URGE_OBJECT(HTTPRequestOptions) {
  std::string method;
  std::string headers;
  scoped_refptr<IOStream> body;
};

/*--urge(name:HTTPRequest)--*/
class URGE_OBJECT(HTTPRequest) {
 public:
  virtual ~HTTPRequest() = default;

  /*--urge(name:OnLoadHandler)--*/
  using OnLoadHandler =
      base::RepeatingCallback<void(scoped_refptr<HTTPRequest> itself)>;

  /*--urge(name:OnErrorHandler)--*/
  using OnErrorHandler =
      base::RepeatingCallback<void(scoped_refptr<HTTPRequest> itself,
                                   const std::string& reason)>;

  /*--urge(name:fetch)--*/
  static scoped_refptr<HTTPRequest> Fetch(
      ExecutionContext* execution_context,
      const std::string& url,
      scoped_refptr<HTTPRequestOptions> options,
      OnLoadHandler onload_callback,
      OnErrorHandler error_callback,
      ExceptionState& exception_state);

  /*--urge(name:abort)--*/
  virtual void Abort(ExceptionState& exception_state) = 0;

  /*--urge(name:status_code)--*/
  virtual int32_t GetStatusCode(ExceptionState& exception_state) = 0;

  /*--urge(name:status_text)--*/
  virtual std::string GetStatusText(ExceptionState& exception_state) = 0;

  /*--urge(name:response_headers)--*/
  virtual std::string GetResponseHeaders(ExceptionState& exception_state) = 0;

  /*--urge(name:response)--*/
  virtual scoped_refptr<IOStream> GetResponse(
      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_HTTPREQUEST_H_
