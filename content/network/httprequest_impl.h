// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_HTTPREQUEST_IMPL_H_
#define CONTENT_NETWORK_HTTPREQUEST_IMPL_H_

#include "asio.hpp"

#include "LUrlParser/LUrlParser.h"

#include "content/context/engine_object.h"
#include "content/io/iostream_impl.h"
#include "content/public/engine_httprequest.h"

namespace content {

struct HTTPRequestAgent;

class HTTPRequestImpl : public HTTPRequest, public EngineObject {
 public:
  HTTPRequestImpl(ExecutionContext* execution_context,
                  LUrlParser::ParseURL url,
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

 private:
  friend struct HTTPRequestAgent;

  HTTPRequestAgent* agent_;

  LUrlParser::ParseURL url_;
  scoped_refptr<HTTPRequestOptions> options_;
  OnLoadHandler onload_callback_;
  OnErrorHandler error_callback_;

  base::WeakPtrFactory<HTTPRequestImpl> weak_ptr_factory_{this};
};

struct HTTPRequestAgent {
  std::unique_ptr<asio::ip::tcp::resolver> resolver_;
  std::unique_ptr<asio::ip::tcp::socket> socket_;

  ExecutionContext* context_;
  base::WeakPtr<HTTPRequestImpl> self_;

  asio::streambuf request_;
  asio::streambuf response_;

  std::string http_version_;
  int32_t status_code_;
  std::string status_text_;
  std::vector<std::string> response_headers_;

  scoped_refptr<IOStreamImpl> response_stream_;

  HTTPRequestAgent(ExecutionContext* execution_context,
                   base::WeakPtr<HTTPRequestImpl> self);

  void CreateRequestInternal();
  void CancelRequestInternal();

  void HandleResolveAsync(
      const std::error_code& err,
      const asio::ip::tcp::resolver::results_type& endpoints);
  void HandleConnect(const std::error_code& err);
  void HandleWriteRequest(const std::error_code& err);
  void HandleReadStatusLine(const std::error_code& err);
  void HandleReadHeaders(const std::error_code& err);
  void HandleReadResponse(const std::error_code& err);

  void OnErrorInternal(const std::error_code& err);
  void OnErrorInternal(const std::string& raw_reason);
};

}  // namespace content

#endif  //! CONTENT_NETWORK_HTTPREQUEST_IMPL_H_
