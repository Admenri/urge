// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NET_HTTPREQUEST_IMPL_H_
#define CONTENT_NET_HTTPREQUEST_IMPL_H_

#include "content/context/engine_object.h"
#include "content/public/engine_httprequest.h"

namespace content {

class HTTPRequestImpl : public HTTPRequest, public EngineObject {
 public:
  HTTPRequestImpl();
  ~HTTPRequestImpl() override;

  HTTPRequestImpl(const HTTPRequestImpl&) = delete;
  HTTPRequestImpl& operator=(const HTTPRequestImpl&) = delete;

  void Open(const std::string& method,
            const std::string& url,
            const std::string& mime_type,
            const std::string& username,
            const std::string& password,
            ExceptionState& exception_state) override;
  void Send(const std::string& body, ExceptionState& exception_state) override;
  void Abort(ExceptionState& exception_state) override;
  void SetTimeout(uint32_t timeout, ExceptionState& exception_state) override;
  void SetRequestHeader(const std::string& key,
                        const std::string& value,
                        ExceptionState& exception_state) override;
  std::string GetResponse(ExceptionState& exception_state) override;
  std::string GetResponseHeader(const std::string& header,
                                ExceptionState& exception_state) override;
  ReadyState GetReadyState(ExceptionState& exception_state) override;
  uint32_t GetStatus(ExceptionState& exception_state) override;
  std::string GetURL(ExceptionState& exception_state) override;
  void SetAbortHandler(ProgressHandler handler,
                       ExceptionState& exception_state) override;
  void SetErrorHandler(ProgressHandler handler,
                       ExceptionState& exception_state) override;
  void SetLoadHandler(ProgressHandler handler,
                      ExceptionState& exception_state) override;
  void SetLoadEndHandler(ProgressHandler handler,
                         ExceptionState& exception_state) override;
  void SetLoadStartHandler(ProgressHandler handler,
                           ExceptionState& exception_state) override;
  void SetProgressHandler(ProgressHandler handler,
                          ExceptionState& exception_state) override;
  void SetReadyStateChangeHandler(ProgressHandler handler,
                                  ExceptionState& exception_state) override;
  void SetTimeoutHandler(ProgressHandler handler,
                         ExceptionState& exception_state) override;

 private:
};

}  // namespace content

#endif  //! CONTENT_NET_HTTPREQUEST_IMPL_H_
