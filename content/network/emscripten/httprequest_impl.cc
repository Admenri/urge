// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/emscripten/httprequest_impl.h"

#include "content/context/execution_context.h"
#include "content/io/iostream_impl.h"

namespace content {

std::vector<std::string> SplitHeaderStrings(
    const std::vector<std::string>& input_strings) {
  std::vector<std::string> result;

  for (const auto& s : input_strings) {
    size_t colon_pos = s.find(':');
    if (colon_pos != std::string::npos) {
      std::string first_part = s.substr(0, colon_pos);
      std::string second_part = s.substr(colon_pos + 1);

      // Trim whitespace from both parts
      first_part.erase(remove(first_part.begin(), first_part.end(), ' '),
                       first_part.end());
      second_part.erase(remove(second_part.begin(), second_part.end(), ' '),
                        second_part.end());

      result.push_back(first_part);
      result.push_back(second_part);
    }
  }

  return result;
}

// static
scoped_refptr<HTTPRequest> HTTPRequest::Fetch(
    ExecutionContext* execution_context,
    const std::string& url,
    scoped_refptr<HTTPRequestOptions> options,
    OnLoadHandler onload_callback,
    OnErrorHandler error_callback,
    ExceptionState& exception_state) {
  // Fetch initializing attributes
  std::unique_ptr<emscripten_fetch_attr_t> fetch_attribute =
      std::make_unique<emscripten_fetch_attr_t>();
  emscripten_fetch_attr_init(fetch_attribute.get());

  // Flags
  fetch_attribute->attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  // Callback
  fetch_attribute->onsuccess = &HTTPRequestImpl::OnSuccess;
  fetch_attribute->onerror = &HTTPRequestImpl::OnError;

  // Method
  std::strcpy(fetch_attribute->requestMethod, options->method.c_str());

  // Headers
  auto raw_headers = SplitHeaderStrings(options->headers);
  char** headers_ptr =
      static_cast<char**>(std::malloc(sizeof(char*) * raw_headers.size() + 1));
  fetch_attribute->requestHeaders = headers_ptr;
  for (const auto& it : raw_headers) {
    *headers_ptr = static_cast<char*>(std::malloc(it.size() + 1));
    std::memcpy(*headers_ptr, it.data(), it.size());
    *headers_ptr[it.size()] = '\0';
    headers_ptr++;
  }
  *headers_ptr = nullptr;

  // Request body
  auto stream_obj = IOStreamImpl::From(options->body);
  if (Disposable::IsValid(stream_obj.get())) {
    fetch_attribute->requestDataSize = SDL_GetIOSize(**stream_obj);
    auto* buf =
        static_cast<char*>(std::malloc(fetch_attribute->requestDataSize));
    fetch_attribute->requestData = buf;
    SDL_ReadIO(**stream_obj, buf, fetch_attribute->requestDataSize);
  }

  // Fetch
  emscripten_fetch_t* connect =
      emscripten_fetch(fetch_attribute.get(), url.c_str());
  if (!connect) {
    if (fetch_attribute->requestData)
      std::free(const_cast<char*>(fetch_attribute->requestData));

    auto* header_raw = const_cast<char**>(fetch_attribute->requestHeaders);
    if (header_raw) {
      for (auto i = header_raw; *i != nullptr; i++)
        std::free(*i);
      std::free(header_raw);
    }

    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to create fetch connection");
    return nullptr;
  }

  return base::MakeRefCounted<HTTPRequestImpl>(
      execution_context, std::move(fetch_attribute), connect, url, options,
      onload_callback, error_callback);
}

HTTPRequestImpl::HTTPRequestImpl(
    ExecutionContext* execution_context,
    std::unique_ptr<emscripten_fetch_attr_t> fetch_attribute,
    emscripten_fetch_t* connect,
    const std::string& url,
    scoped_refptr<HTTPRequestOptions> options,
    OnLoadHandler onload_callback,
    OnErrorHandler error_callback)
    : EngineObject(execution_context),
      fetch_attribute_(std::move(fetch_attribute)),
      connect_(connect),
      url_(url),
      options_(options),
      onload_callback_(onload_callback),
      error_callback_(error_callback) {
  fetch_attribute_->userData = this;
}

HTTPRequestImpl::~HTTPRequestImpl() {
  if (connect_)
    emscripten_fetch_close(connect_);

  if (fetch_attribute_->requestData)
    std::free((char*)fetch_attribute_->requestData);

  auto* header_raw = const_cast<char**>(fetch_attribute_->requestHeaders);
  if (header_raw) {
    for (auto i = header_raw; *i != nullptr; i++)
      std::free(*i);
    std::free(header_raw);
  }
}

void HTTPRequestImpl::Abort(ExceptionState& exception_state) {
  if (connect_) {
    emscripten_fetch_close(connect_);
    connect_ = nullptr;
  }
}

int32_t HTTPRequestImpl::GetStatusCode(ExceptionState& exception_state) {
  if (!connect_)
    return -1;
  return connect_->status;
}

std::string HTTPRequestImpl::GetStatusText(ExceptionState& exception_state) {
  if (!connect_)
    return {};
  return connect_->statusText;
}

std::vector<std::string> HTTPRequestImpl::GetResponseHeaders(
    ExceptionState& exception_state) {
  if (!connect_)
    return {};

  size_t header_length =
      emscripten_fetch_get_response_headers_length(connect_) + 1;
  auto* header_string = static_cast<char*>(std::malloc(header_length));

  DCHECK(header_string);
  emscripten_fetch_get_response_headers(connect_, header_string, header_length);
  char** response_headers =
      emscripten_fetch_unpack_response_headers(header_string);
  std::free(header_string);

  std::vector<std::string> headers;
  int num_headers = 0;
  for (; response_headers[num_headers * 2]; ++num_headers) {
    // Check both the header and its value are present.
    DCHECK(response_headers[num_headers * 2 + 1]);
    headers.push_back(std::string(response_headers[num_headers * 2]) + ": " +
                      std::string(response_headers[num_headers * 2 + 1]));
  }

  emscripten_fetch_free_unpacked_response_headers(response_headers);
  return headers;
}

scoped_refptr<IOStream> HTTPRequestImpl::GetResponse(
    ExceptionState& exception_state) {
  if (!connect_ || !connect_->data)
    return nullptr;

  auto* raw_stream = SDL_IOFromDynamicMem();
  SDL_WriteIO(raw_stream, connect_->data, connect_->numBytes);

  return base::MakeRefCounted<IOStreamImpl>(context(), raw_stream);
}

void HTTPRequestImpl::OnSuccess(emscripten_fetch_t* fetch) {}

void HTTPRequestImpl::OnError(emscripten_fetch_t* fetch) {}

}  // namespace content
