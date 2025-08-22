// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NET_FETCH_IMPL_H_
#define CONTENT_NET_FETCH_IMPL_H_

#include <map>

#include "asio.hpp"

#include "content/context/engine_object.h"
#include "content/net/network_context.h"
#include "content/public/engine_fetch.h"

namespace content {

class FetchImpl : public Fetch, public EngineObject {
 public:
  FetchImpl(ExecutionContext* execution_context);
  ~FetchImpl() override;

  FetchImpl(const FetchImpl&) = delete;
  FetchImpl& operator=(const FetchImpl&) = delete;

  void Send(ExceptionState& exception_state) override;
  void Abort(ExceptionState& exception_state) override;
  ReadyState GetReadyState(ExceptionState& exception_state) override;
  void SetResponseHandler(ResponseHandler handler,
                          ExceptionState& exception_state) override;
  void SetProgressHandler(ProgressHandler handler,
                          ExceptionState& exception_state) override;

 private:
  asio::io_context* context_;
  asio::ip::tcp::resolver resolver_;
  asio::ip::tcp::socket socket_;

  scoped_refptr<HTTPRequest> request_;
  scoped_refptr<HTTPResponse> response_;
  asio::streambuf request_buffer_;
  asio::streambuf response_buffer_;

  std::atomic<ReadyState> ready_state_;
  ResponseHandler on_response_;
  ProgressHandler on_progress_;
};

}  // namespace content

#endif  //! CONTENT_NET_FETCH_IMPL_H_
