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

#include "core/network_backend.h"

#include <emscripten/fetch.h>
#include <emscripten/websocket.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace urge {

namespace {

RefPtr<NetworkEvent> MakeEvent(int32_t type) {
  auto event = MakeRefCounted<NetworkEvent>();
  event->type_ = type;
  return event;
}

/* The browser delivers every WebSocket callback on the main thread, so there
   is no worker here: the callbacks write into the same worker queue the native
   backend fills, and `Swap` moves it across exactly as it does there. */
class WebWebSocketImpl final : public WebSocketImpl {
 public:
  WebWebSocketImpl(NetworkBackend* backend, std::string url,
                   std::vector<std::string> protocols)
      : backend_(backend),
        url_(std::move(url)),
        protocols_(std::move(protocols)) {}

  ~WebWebSocketImpl() override {
    if (backend_)
      backend_->Untrack(this);

    Stop();
  }

  void Open() {
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url_.c_str();
    attributes.createOnMainThread = true;

    std::string joined;
    for (std::size_t i = 0; i < protocols_.size(); ++i) {
      if (i)
        joined += ",";
      joined += protocols_[i];
    }
    attributes.protocols = joined.empty() ? nullptr : joined.c_str();

    socket_ = emscripten_websocket_new(&attributes);
    if (socket_ <= 0) {
      Finish(WebSocket::ClosedState, "the websocket could not be created");
      PushEvent(MakeEvent(NetworkEvent::Error));
      socket_ = -1;
      return;
    }

    emscripten_websocket_set_onopen_callback(socket_, this, OnOpen);
    emscripten_websocket_set_onmessage_callback(socket_, this, OnMessage);
    emscripten_websocket_set_onclose_callback(socket_, this, OnClose);
    emscripten_websocket_set_onerror_callback(socket_, this, OnError);
  }

  void Poll(WebSocket& self) override {
    self.ready_state_ = ready_state_;
    self.dropped_ = dropped_;
    if (!error_.empty())
      self.error_ = error_;
  }

  void Swap() override {
    while (!worker_queue_.empty()) {
      main_queue_.push_back(std::move(worker_queue_.front()));
      worker_queue_.pop_front();
    }

    while (static_cast<int32_t>(main_queue_.size()) > max_events_) {
      auto it = std::find_if(main_queue_.begin(), main_queue_.end(),
                             [](const RefPtr<NetworkEvent>& event) {
                               return event->type_ != NetworkEvent::Close &&
                                      event->type_ != NetworkEvent::Error;
                             });
      if (it == main_queue_.end())
        break;

      main_queue_.erase(it);
      ++dropped_;
    }
  }

  void SetMaxEvents(int32_t max_events) override { max_events_ = max_events; }

  RefPtr<NetworkEvent> NextEvent() override {
    if (main_queue_.empty())
      return nullptr;

    auto event = std::move(main_queue_.front());
    main_queue_.pop_front();
    return event;
  }

  int32_t Pending() override { return static_cast<int32_t>(main_queue_.size()); }

  void SendText(const std::string& data) override {
    if (ready_state_ != WebSocket::OpenState)
      return;

    emscripten_websocket_send_utf8_text(socket_, data.c_str());
  }

  void SendBinary(const std::string& data) override {
    if (ready_state_ != WebSocket::OpenState)
      return;

    emscripten_websocket_send_binary(socket_,
                                     const_cast<char*>(data.data()),
                                     static_cast<uint32_t>(data.size()));
  }

  void SendPing(const std::string& data) override {}

  void Close(int32_t code, const std::string& reason) override {
    if (socket_ < 0)
      return;

    emscripten_websocket_close(socket_, static_cast<unsigned short>(code),
                               reason.c_str());
  }

  void Stop() {
    if (socket_ < 0)
      return;

    stopped_ = true;

    emscripten_websocket_set_onopen_callback(socket_, nullptr, nullptr);
    emscripten_websocket_set_onmessage_callback(socket_, nullptr, nullptr);
    emscripten_websocket_set_onclose_callback(socket_, nullptr, nullptr);
    emscripten_websocket_set_onerror_callback(socket_, nullptr, nullptr);

    emscripten_websocket_close(socket_, 1000, "");
    emscripten_websocket_delete(socket_);
    socket_ = -1;
  }

 private:
  static bool OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* user) {
    auto* self = static_cast<WebWebSocketImpl*>(user);
    if (self->stopped_)
      return true;

    self->HandleOpen();
    return true;
  }

  static bool OnMessage(int, const EmscriptenWebSocketMessageEvent* event,
                        void* user) {
    auto* self = static_cast<WebWebSocketImpl*>(user);
    if (self->stopped_)
      return true;

    self->HandleMessage(event);
    return true;
  }

  static bool OnClose(int, const EmscriptenWebSocketCloseEvent* event,
                      void* user) {
    auto* self = static_cast<WebWebSocketImpl*>(user);
    if (self->stopped_)
      return true;

    self->HandleClose(event);
    return true;
  }

  static bool OnError(int, const EmscriptenWebSocketErrorEvent*, void* user) {
    auto* self = static_cast<WebWebSocketImpl*>(user);
    if (self->stopped_)
      return true;

    self->HandleError();
    return true;
  }

  void HandleOpen() {
    ready_state_ = WebSocket::OpenState;
    worker_queue_.push_back(MakeEvent(NetworkEvent::Open));
  }

  void HandleMessage(const EmscriptenWebSocketMessageEvent* event) {
    const std::string data(reinterpret_cast<const char*>(event->data),
                           event->numBytes);
    if (event->isText) {
      auto text = MakeEvent(NetworkEvent::Text);
      text->text_ = data;
      text->data_ = data;
      worker_queue_.push_back(std::move(text));
    } else {
      auto binary = MakeEvent(NetworkEvent::Binary);
      binary->data_ = data;
      worker_queue_.push_back(std::move(binary));
    }
  }

  void HandleClose(const EmscriptenWebSocketCloseEvent* event) {
    auto close = MakeEvent(NetworkEvent::Close);
    close->code_ = event->code;
    close->reason_ = event->reason;
    worker_queue_.push_back(std::move(close));
    ready_state_ = WebSocket::ClosedState;
  }

  void HandleError() {
    worker_queue_.push_back(MakeEvent(NetworkEvent::Error));
    ready_state_ = WebSocket::ClosedState;
    error_ = "the websocket reported an error";
  }

  void PushEvent(RefPtr<NetworkEvent> event) {
    worker_queue_.push_back(std::move(event));
  }

  void Finish(int32_t state, const std::string& error) {
    ready_state_ = state;
    error_ = error;
  }

  NetworkBackend* backend_ = nullptr;
  std::string url_;
  std::vector<std::string> protocols_;
  EMSCRIPTEN_WEBSOCKET_T socket_ = -1;
  bool stopped_ = false;
  int32_t ready_state_ = WebSocket::Connecting;
  std::string error_;
  int32_t dropped_ = 0;
  int32_t max_events_ = 1024;

  std::deque<RefPtr<NetworkEvent>> worker_queue_;
  std::deque<RefPtr<NetworkEvent>> main_queue_;
};

class WebFetchImpl final : public FetchImpl {
 public:
  WebFetchImpl(std::string url, std::string method, std::string body,
               std::vector<std::string> headers)
      : url_(std::move(url)),
        method_(std::move(method)),
        body_(std::move(body)),
        headers_(std::move(headers)) {}

  ~WebFetchImpl() override {
    if (fetch_)
      emscripten_fetch_close(fetch_);
  }

  void Start() override {
    if (started_)
      return;

    started_ = true;

    for (const auto& line : headers_) {
      const auto colon = line.find(':');
      if (colon == std::string::npos)
        continue;

      std::string key = line.substr(0, colon);
      std::string value = line.substr(colon + 1);
      const auto first = value.find_first_not_of(' ');
      if (first != std::string::npos)
        value = value.substr(first);

      request_headers_.push_back(std::move(key));
      request_headers_.push_back(std::move(value));
    }

    for (auto& header : request_headers_)
      header_pointers_.push_back(header.c_str());
    header_pointers_.push_back(nullptr);

    emscripten_fetch_attr_t attributes;
    emscripten_fetch_attr_init(&attributes);
    std::snprintf(attributes.requestMethod, sizeof(attributes.requestMethod),
                  "%s", method_.c_str());
    attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attributes.onsuccess = OnSuccess;
    attributes.onerror = OnError;
    attributes.userData = this;
    if (header_pointers_.size() > 1)
      attributes.requestHeaders = header_pointers_.data();
    if (!body_.empty()) {
      attributes.requestData = body_.data();
      attributes.requestDataSize = body_.size();
    }

    fetch_ = emscripten_fetch(&attributes, url_.c_str());
    if (!fetch_)
      Finish("the fetch could not be started", 0, std::string());
  }

  void Abort() override {
    aborted_ = true;
    if (fetch_)
      emscripten_fetch_close(fetch_);
    fetch_ = nullptr;
  }

  void Poll(Fetch& self) override {
    if (!done_)
      return;

    self.body_ = body_result_;
    self.status_ = status_;
    self.status_text_ = status_text_;
    self.error_ = error_;
    self.headers_ = headers_result_;
    self.done_ = true;
    self.success_ = error_.empty() && status_ >= 200 && status_ < 300;
    self.progress_ = 1.0f;

    done_ = false;
  }

 private:
  static void OnSuccess(emscripten_fetch_t* fetch) {
    auto* self = static_cast<WebFetchImpl*>(fetch->userData);
    if (self->aborted_)
      return;

    self->HandleSuccess(fetch);
  }

  static void OnError(emscripten_fetch_t* fetch) {
    auto* self = static_cast<WebFetchImpl*>(fetch->userData);
    if (self->aborted_)
      return;

    self->HandleError(fetch);
  }

  void HandleSuccess(emscripten_fetch_t* fetch) {
    body_result_.assign(fetch->data, fetch->numBytes);
    status_ = fetch->status;
    status_text_ = fetch->statusText;
    ParseHeaders(fetch);
    error_.clear();
    done_ = true;
  }

  void HandleError(emscripten_fetch_t* fetch) {
    error_ = "the request failed";
    status_ = fetch->status;
    status_text_ = fetch->statusText;
    done_ = true;
  }

  void ParseHeaders(emscripten_fetch_t* fetch) {
    const size_t length = emscripten_fetch_get_response_headers_length(fetch);
    std::string raw(length, '\0');
    emscripten_fetch_get_response_headers(fetch, raw.data(), length + 1);

    std::size_t start = 0;
    while (start < raw.size()) {
      auto end = raw.find("\r\n", start);
      if (end == std::string::npos)
        end = raw.size();
      if (end > start)
        headers_result_.push_back(raw.substr(start, end - start));
      start = end + 2;
    }
  }

  void Finish(const char* message, int32_t status, const std::string& text) {
    error_ = message;
    status_ = status;
    status_text_ = text;
    done_ = true;
  }

  std::string url_;
  std::string method_;
  std::string body_;
  std::vector<std::string> headers_;

  std::vector<std::string> request_headers_;
  std::vector<const char*> header_pointers_;

  bool started_ = false;
  bool done_ = false;
  bool aborted_ = false;

  emscripten_fetch_t* fetch_ = nullptr;

  std::string body_result_;
  int32_t status_ = 0;
  std::string status_text_;
  std::string error_;
  std::vector<std::string> headers_result_;
};

class WebNetworkBackend final : public NetworkBackend {
 public:
  void GlobalInit() override {}

  void GlobalCleanup() override {}

  void Update() override {
    for (auto* socket : sockets_)
      socket->Swap();
  }

  void SetMaxEvents(int32_t max_events) override {
    max_events_ = max_events;
    for (auto* socket : sockets_)
      socket->SetMaxEvents(max_events_);
  }

  RefPtr<WebSocket> OpenWebSocket(
      const std::string& url,
      const std::vector<std::string>& protocols) override {
    auto socket = MakeRefCounted<WebSocket>(url, protocols);
    auto impl = std::make_unique<WebWebSocketImpl>(this, url, protocols);
    impl->SetMaxEvents(max_events_);
    WebWebSocketImpl* raw = impl.get();
    socket->Attach(std::move(impl));
    sockets_.push_back(raw);
    raw->Open();
    return socket;
  }

  RefPtr<Fetch> OpenFetch(const std::string& url, const std::string& method,
                          const std::string& body,
                          const std::vector<std::string>& headers) override {
    auto fetch = MakeRefCounted<Fetch>(url, method, body, headers);
    fetch->Attach(std::make_unique<WebFetchImpl>(url, method, body, headers));
    return fetch;
  }

  void CloseAll() override {
    for (auto* socket : sockets_)
      socket->Stop();
    sockets_.clear();
  }

  void Untrack(WebSocketImpl* impl) override {
    auto* socket = static_cast<WebWebSocketImpl*>(impl);
    sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket),
                   sockets_.end());
  }

 private:
  std::vector<WebWebSocketImpl*> sockets_;
  int32_t max_events_ = 1024;
};

}  // namespace

std::unique_ptr<NetworkBackend> CreateNetworkBackend() {
  return std::make_unique<WebNetworkBackend>();
}

}  // namespace urge
