// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_EMSCRIPTEN_HTTPREQUEST_IMPL_H_
#define CONTENT_NETWORK_EMSCRIPTEN_HTTPREQUEST_IMPL_H_

#include <emscripten.h>
#include <emscripten/fetch.h>

#include "content/context/engine_object.h"
#include "content/io/iostream_impl.h"
#include "content/public/engine_httprequest.h"

namespace content {

struct HTTPRequestAgent;

class HTTPRequestImpl : public HTTPRequest, public EngineObject {
 public:
  HTTPRequestImpl(ExecutionContext* execution_context,
                  std::unique_ptr<emscripten_fetch_attr_t> fetch_attribute,
                  emscripten_fetch_t* connect,
                  const std::string& url,
                  scoped_refptr<HTTPRequestOptions> options,
                  OnLoadHandler onload_callback,
                  OnErrorHandler error_callback);
  ~HTTPRequestImpl();

  HTTPRequestImpl(const HTTPRequestImpl&) = delete;
  HTTPRequestImpl& operator=(const HTTPRequestImpl&) = delete;

 public:
  void Abort(ExceptionState& exception_state) override;
  int32_t GetStatusCode(ExceptionState& exception_state) override;
  std::string GetStatusText(ExceptionState& exception_state) override;
  std::vector<std::string> GetResponseHeaders(
      ExceptionState& exception_state) override;
  scoped_refptr<IOStream> GetResponse(ExceptionState& exception_state) override;

  static void OnSuccess(emscripten_fetch_t* fetch);
  static void OnError(emscripten_fetch_t* fetch);

 private:
  std::unique_ptr<emscripten_fetch_attr_t> fetch_attribute_;
  emscripten_fetch_t* connect_;

  std::string url_;
  scoped_refptr<HTTPRequestOptions> options_;
  OnLoadHandler onload_callback_;
  OnErrorHandler error_callback_;
};

}  // namespace content

#endif  //! CONTENT_NETWORK_HTTPREQUEST_IMPL_H_
