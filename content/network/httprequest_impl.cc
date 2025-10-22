// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/httprequest_impl.h"

// Definition
#include "LUrlParser/LUrlParser.cpp"

#include "content/context/execution_context.h"

namespace content {

// static
scoped_refptr<HTTPRequest> HTTPRequest::Fetch(
    ExecutionContext* execution_context,
    const std::string& url,
    scoped_refptr<HTTPRequestOptions> options,
    OnLoadHandler onload_callback,
    OnErrorHandler error_callback,
    ExceptionState& exception_state) {
  const auto parsed_url = LUrlParser::ParseURL::parseURL(url);
  if (!parsed_url.isValid()) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "invalid url: %s",
                               url.c_str());
    return nullptr;
  }

  return base::MakeRefCounted<HTTPRequestImpl>(execution_context,
                                               std::move(parsed_url), options,
                                               onload_callback, error_callback);
}

HTTPRequestImpl::HTTPRequestImpl(ExecutionContext* execution_context,
                                 LUrlParser::ParseURL url,
                                 scoped_refptr<HTTPRequestOptions> options,
                                 OnLoadHandler onload_callback,
                                 OnErrorHandler error_callback)
    : EngineObject(execution_context),
      url_(std::move(url)),
      options_(options),
      onload_callback_(onload_callback),
      error_callback_(error_callback) {
  agent_ =
      new HTTPRequestAgent(execution_context, weak_ptr_factory_.GetWeakPtr());
}

HTTPRequestImpl::~HTTPRequestImpl() {
  context()->network_service->PostNetworkTask(
      base::BindOnce([](HTTPRequestAgent* agent) { delete agent; }, agent_));
}

void HTTPRequestImpl::Abort(ExceptionState& exception_state) {
  context()->network_service->PostNetworkTask(base::BindOnce(
      &HTTPRequestAgent::CancelRequestInternal, base::Unretained(agent_)));
}

int32_t HTTPRequestImpl::GetStatusCode(ExceptionState& exception_state) {
  return agent_->status_code_;
}

std::string HTTPRequestImpl::GetStatusText(ExceptionState& exception_state) {
  return agent_->status_text_;
}

std::vector<std::string> HTTPRequestImpl::GetResponseHeaders(
    ExceptionState& exception_state) {
  return agent_->response_headers_;
}

scoped_refptr<IOStream> HTTPRequestImpl::GetResponse(
    ExceptionState& exception_state) {
  return agent_->response_stream_;
}

HTTPRequestAgent::HTTPRequestAgent(ExecutionContext* execution_context,
                                   base::WeakPtr<HTTPRequestImpl> self)
    : context_(execution_context), self_(self) {
  std::string path = "/";
  if (!self_->url_.path_.empty())
    path = self_->url_.path_;

  std::ostream request_stream(&request_);
  request_stream << self_->options_->method << " " << path << " HTTP/1.0\r\n";
  request_stream << "Host: " << self_->url_.host_ << "\r\n";
  request_stream << "Accept: */*\r\n";
  request_stream << "Connection: close\r\n";

  for (const auto& it : self_->options_->headers)
    request_stream << it;

  request_stream << "\r\n";

  auto stream_obj = IOStreamImpl::From(self_->options_->body);
  if (Disposable::IsValid(stream_obj.get())) {
    auto* raw_stream = **stream_obj;
    std::string buf(SDL_GetIOSize(raw_stream), 0);
    SDL_ReadIO(raw_stream, buf.data(), buf.size());
    request_stream.write(buf.data(), buf.size());
  }

  context_->network_service->PostNetworkTask(base::BindOnce(
      &HTTPRequestAgent::CreateRequestInternal, base::Unretained(this)));
}

void HTTPRequestAgent::CreateRequestInternal() {
  // Network service context
  auto* io_context =
      static_cast<asio::io_context*>(context_->network_service->GetIOContext());

  resolver_ = std::make_unique<asio::ip::tcp::resolver>(*io_context);
  socket_ = std::make_unique<asio::ip::tcp::socket>(*io_context);

  resolver_->async_resolve(
      self_->url_.host_, self_->url_.scheme_,
      std::bind(&HTTPRequestAgent::HandleResolveAsync, this,
                asio::placeholders::error, asio::placeholders::results));
}

void HTTPRequestAgent::CancelRequestInternal() {
  socket_->cancel();
}

void HTTPRequestAgent::HandleResolveAsync(
    const std::error_code& err,
    const asio::ip::tcp::resolver::results_type& endpoints) {
  if (!err) {
    // Attempt a connection to each endpoint in the list until we
    // successfully establish a connection.
    asio::async_connect(*socket_, endpoints,
                        std::bind(&HTTPRequestAgent::HandleConnect, this,
                                  asio::placeholders::error));
  } else {
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::HandleConnect(const std::error_code& err) {
  if (!err) {
    // The connection was successful. Send the request.
    asio::async_write(*socket_, request_,
                      std::bind(&HTTPRequestAgent::HandleWriteRequest, this,
                                asio::placeholders::error));
  } else {
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::HandleWriteRequest(const std::error_code& err) {
  if (!err) {
    // Read the response status line. The response_ streambuf will
    // automatically grow to accommodate the entire line. The growth may be
    // limited by passing a maximum size to the streambuf constructor.
    asio::async_read_until(*socket_, response_, "\r\n",
                           std::bind(&HTTPRequestAgent::HandleReadStatusLine,
                                     this, asio::placeholders::error));
  } else {
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::HandleReadStatusLine(const std::error_code& err) {
  if (!err) {
    // Check that response is OK.
    std::istream response_stream(&response_);

    response_stream >> http_version_;
    response_stream >> status_code_;
    std::getline(response_stream, status_text_);

    if (!response_stream || http_version_.substr(0, 5) != "HTTP/")
      return OnErrorInternal("invalid http header: " + http_version_);

    if (status_code_ != 200)
      return OnErrorInternal("invalid status code: " +
                             std::to_string(status_code_));

    // Read the response headers, which are terminated by a blank line.
    asio::async_read_until(*socket_, response_, "\r\n\r\n",
                           std::bind(&HTTPRequestAgent::HandleReadHeaders, this,
                                     asio::placeholders::error));
  } else {
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::HandleReadHeaders(const std::error_code& err) {
  if (!err) {
    // Process the response headers.
    std::istream response_stream(&response_);

    std::string header;
    while (std::getline(response_stream, header) && header != "\r") {
      if (header.back() == '\r')
        header.pop_back();
      response_headers_.push_back(header);
    }

    // Start reading remaining data until EOF.
    asio::async_read(*socket_, response_, asio::transfer_at_least(1),
                     std::bind(&HTTPRequestAgent::HandleReadResponse, this,
                               asio::placeholders::error));
  } else {
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::HandleReadResponse(const std::error_code& err) {
  if (!err) {
    // Continue reading remaining data until EOF.
    asio::async_read(*socket_, response_, asio::transfer_at_least(1),
                     std::bind(&HTTPRequestAgent::HandleReadResponse, this,
                               asio::placeholders::error));
  } else if (err == asio::error::eof) {
    std::stringstream buf;
    buf << &response_;

    // Create stream
    auto* raw_stream = SDL_IOFromDynamicMem();
    response_stream_ = base::MakeRefCounted<IOStreamImpl>(context_, raw_stream);

    // Copy response
    auto buf_data = buf.str();
    SDL_WriteIO(**response_stream_, buf_data.data(), buf_data.size());

    // On load
    if (self_)
      context_->network_service->PushEvent(base::BindRepeating(
          self_->onload_callback_, scoped_refptr(self_.get())));
  } else {
    // On error
    OnErrorInternal(err);
  }
}

void HTTPRequestAgent::OnErrorInternal(const std::error_code& err) {
  if (!self_)
    return;

  context_->network_service->PushEvent(base::BindRepeating(
      self_->error_callback_, scoped_refptr(self_.get()), err.message()));
}

void HTTPRequestAgent::OnErrorInternal(const std::string& raw_reason) {
  if (!self_)
    return;

  context_->network_service->PushEvent(base::BindRepeating(
      self_->error_callback_, scoped_refptr(self_.get()), raw_reason));
}

}  // namespace content
