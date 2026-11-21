// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/network/network.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "core/common/exception.h"
#include "core/network/network_backend.h"

namespace urge {

namespace {

#if !URGE_ENABLE_NETWORK
void NetworkUnavailable(URGE_EXCEPT) {
  exception_state.Throw(ExceptionState::kRGSSError,
                        "network is not available in this build.");
}
#endif

std::string FindHeader(const std::vector<std::string>& lines,
                       const std::string& name) {
  for (const auto& line : lines) {
    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;

    std::string key = line.substr(0, colon);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
      key.pop_back();
    if (key.size() != name.size())
      continue;
    if (!std::equal(key.begin(), key.end(), name.begin(), [](char a, char b) {
          return std::tolower(static_cast<unsigned char>(a)) ==
                 std::tolower(static_cast<unsigned char>(b));
        }))
      continue;

    std::string value = line.substr(colon + 1);
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos)
      return std::string();

    value = value.substr(first);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
      value.pop_back();
    return value;
  }
  return std::string();
}

}  // namespace

NetworkEvent::NetworkEvent() = default;

NetworkEvent::~NetworkEvent() = default;

int32_t NetworkEvent::GetType() {
  return type_;
}

std::string NetworkEvent::GetText() {
  return text_;
}

int32_t NetworkEvent::GetCode() {
  return code_;
}

std::string NetworkEvent::GetReason() {
  return reason_;
}

std::string NetworkEvent::GetErrorMessage() {
  return message_;
}

WebSocket::WebSocket(std::string url, std::vector<std::string> protocols)
    : url_(std::move(url)), protocols_(std::move(protocols)) {
#if URGE_ENABLE_NETWORK
  if (auto* network = Network::TryGet(); network && network->backend_) {
    Attach(network->backend_->CreateWebSocketImpl(url_, protocols_,
                                                  network->timeout_));
    return;
  }
#endif
  ready_state_ = ClosedState;
  error_ = "network is not available in this build.";
}

WebSocket::~WebSocket() = default;

void WebSocket::Attach(std::unique_ptr<WebSocketImpl> impl) {
  impl_ = std::move(impl);
}

std::string WebSocket::GetUrl() {
  return url_;
}

int32_t WebSocket::GetReadyState() {
  if (impl_)
    impl_->Poll(*this);
  return ready_state_;
}

std::string WebSocket::GetError() {
  if (impl_)
    impl_->Poll(*this);
  return error_;
}

int32_t WebSocket::GetDropped() {
  if (impl_)
    impl_->Poll(*this);
  return dropped_;
}

void WebSocket::SendText(std::string data) {
  if (impl_)
    impl_->SendText(data);
}

void WebSocket::SendBinary(std::string data) {
  if (impl_)
    impl_->SendBinary(data);
}

void WebSocket::SendPing(std::string data) {
  if (impl_)
    impl_->SendPing(data);
}

void WebSocket::Close(int32_t code, std::string reason) {
  if (impl_)
    impl_->Close(code, reason);
}

RefPtr<NetworkEvent> WebSocket::NextEvent() {
  if (!impl_)
    return nullptr;

  impl_->Poll(*this);
  return impl_->NextEvent();
}

int32_t WebSocket::GetPending() {
  if (!impl_)
    return 0;

  impl_->Poll(*this);
  return impl_->Pending();
}

Fetch::Fetch(std::string url,
             std::string method,
             std::string body,
             std::vector<std::string> headers)
    : url_(std::move(url)),
      method_(std::move(method)),
      request_body_(std::move(body)),
      request_headers_(std::move(headers)) {
#if URGE_ENABLE_NETWORK
  if (auto* network = Network::TryGet(); network && network->backend_) {
    Attach(network->backend_->CreateFetchImpl(
        url_, method_, request_body_, request_headers_, network->timeout_));
    return;
  }
#endif
  done_ = true;
  error_ = "network is not available in this build.";
}

Fetch::~Fetch() = default;

void Fetch::Attach(std::unique_ptr<FetchImpl> impl) {
  impl_ = std::move(impl);
}

void Fetch::Start() {
  if (impl_)
    impl_->Start();
}

void Fetch::Abort() {
  if (impl_)
    impl_->Abort();
}

bool Fetch::IsDone() {
  if (impl_)
    impl_->Poll(*this);
  return done_;
}

bool Fetch::IsSuccess() {
  if (impl_)
    impl_->Poll(*this);
  return success_;
}

int32_t Fetch::GetStatus() {
  if (impl_)
    impl_->Poll(*this);
  return status_;
}

std::string Fetch::GetStatusText() {
  if (impl_)
    impl_->Poll(*this);
  return status_text_;
}

std::string Fetch::GetText() {
  if (impl_)
    impl_->Poll(*this);
  return body_;
}

const std::string& Fetch::body() {
  if (impl_)
    impl_->Poll(*this);
  return body_;
}

std::string Fetch::GetError() {
  if (impl_)
    impl_->Poll(*this);
  return error_;
}

float Fetch::GetProgress() {
  if (impl_)
    impl_->Poll(*this);
  return progress_;
}

std::string Fetch::GetHeader(std::string name) {
  if (impl_)
    impl_->Poll(*this);
  return FindHeader(headers_, name);
}

std::vector<std::string> Fetch::GetHeaders() {
  if (impl_)
    impl_->Poll(*this);
  return headers_;
}

Network::Network() {
#if URGE_ENABLE_NETWORK
  backend_ = CreateNetworkBackend();
  backend_->GlobalInit();
#endif
}

Network::~Network() {
#if URGE_ENABLE_NETWORK
  CloseAll();
  backend_->GlobalCleanup();
#endif
}

void Network::Update() {
#if URGE_ENABLE_NETWORK
  backend_->Update();
#endif
}

void Network::CloseAll() {
#if URGE_ENABLE_NETWORK
  backend_->CloseAll();
#endif
}

RefPtr<WebSocket> Network::OpenWebSocket(URGE_EXCEPT,
                                         std::string url,
                                         std::vector<std::string> protocols) {
#if URGE_ENABLE_NETWORK
  return MakeRefCounted<WebSocket>(std::move(url), std::move(protocols));
#else
  NetworkUnavailable(exception_state);
  return nullptr;
#endif
}

RefPtr<Fetch> Network::OpenFetch(URGE_EXCEPT,
                                 std::string url,
                                 std::string method,
                                 std::string body,
                                 std::vector<std::string> headers) {
#if URGE_ENABLE_NETWORK
  return MakeRefCounted<Fetch>(std::move(url), std::move(method),
                               std::move(body), std::move(headers));
#else
  NetworkUnavailable(exception_state);
  return nullptr;
#endif
}

ATTR_DEF(Network, int32_t, Timeout) {
  if (value.has_value()) {
    timeout_ = std::max(*value, 1);
    return std::nullopt;
  }

  return timeout_;
}

ATTR_DEF(Network, int32_t, MaxEvents) {
  if (value.has_value()) {
    max_events_ = std::max(*value, 1);
#if URGE_ENABLE_NETWORK
    backend_->SetMaxEvents(max_events_);
#endif
    return std::nullopt;
  }

  return max_events_;
}

}  // namespace urge
