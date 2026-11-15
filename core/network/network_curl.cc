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

#include "core/common/persistent.h"
#include "core/network/network_backend.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

namespace urge {

namespace {

std::size_t OnWrite(char* data,
                    std::size_t size,
                    std::size_t count,
                    void* user) {
  auto* out = static_cast<std::string*>(user);
  const std::size_t total = size * count;
  out->append(data, total);
  return total;
}

Persistent<NetworkEvent> MakeEvent(int32_t type) {
  // Keep the worker's raw factory result protected until its root is registered.
  GarbageCollector::ConstructionScope construction;
  Persistent<NetworkEvent> event = MakeGarbageCollected<NetworkEvent>();
  event->type_ = type;
  return event;
}

// libcurl's mbedTLS backend checks the platform's CA directory, and Android
// keeps its system store as a directory of hashed PEM files rather than the
// `/etc/ssl/certs/ca-certificates.crt` bundle the backend looks for by
// default.  Without this every https and wss connection fails verification
// because the compiled-in path simply does not exist.
void ConfigureTls(CURL* handle) {
#if defined(OS_ANDROID)
  curl_easy_setopt(handle, CURLOPT_CAPATH, "/system/etc/security/cacerts");
#else
  static_cast<void>(handle);
#endif
}

class CurlWebSocketImpl final : public WebSocketImpl {
 public:
  CurlWebSocketImpl(NetworkBackend* backend,
                    std::string url,
                    std::vector<std::string> protocols)
      : backend_(backend),
        url_(std::move(url)),
        protocols_(std::move(protocols)) {}

  ~CurlWebSocketImpl() override {
    if (backend_)
      backend_->Untrack(this);

    Stop();
  }

  void Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_ || stopping_.load(std::memory_order_relaxed))
      return;

    started_ = true;
    worker_ = std::thread(&CurlWebSocketImpl::Run, this);
  }

  void DetachBackend() {
    backend_ = nullptr;
    Stop();
  }

  void Stop() {
    stopping_.store(true, std::memory_order_relaxed);
    if (worker_.joinable())
      worker_.join();

    std::lock_guard<std::mutex> lock(mutex_);
    ready_state_ = WebSocket::ClosedState;
    outgoing_.clear();
  }

  void Poll(WebSocket& self) override {
    std::lock_guard<std::mutex> lock(mutex_);

    self.ready_state_ = ready_state_;
    self.dropped_ = dropped_;
    if (!error_.empty())
      self.error_ = error_;
  }

  void Swap() override {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!worker_queue_.empty()) {
      main_queue_.push_back(std::move(worker_queue_.front()));
      worker_queue_.pop_front();
    }

    while (static_cast<int32_t>(main_queue_.size()) > max_events_) {
      auto it = std::find_if(main_queue_.begin(), main_queue_.end(),
                             [](const Persistent<NetworkEvent>& event) {
                               return event->type_ != NetworkEvent::Close &&
                                      event->type_ != NetworkEvent::Error;
                             });
      if (it == main_queue_.end())
        break;

      main_queue_.erase(it);
      ++dropped_;
    }
  }

  void SetMaxEvents(int32_t max_events) override {
    std::lock_guard<std::mutex> lock(mutex_);
    max_events_ = max_events;
  }

  NetworkEvent* NextEvent() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (main_queue_.empty())
      return nullptr;

    auto event = std::move(main_queue_.front());
    main_queue_.pop_front();
    return event.get();
  }

  int32_t Pending() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int32_t>(main_queue_.size());
  }

  void SendText(const std::string& data) override {
    QueueSend(data, CURLWS_TEXT);
  }

  void SendBinary(const std::string& data) override {
    QueueSend(data, CURLWS_BINARY);
  }

  void SendPing(const std::string& data) override {
    QueueSend(data, CURLWS_PING);
  }

  void Close(int32_t code, const std::string& reason) override {
    std::string payload;
    payload.push_back(static_cast<char>((code >> 8) & 0xff));
    payload.push_back(static_cast<char>(code & 0xff));
    payload += reason;
    QueueSend(payload, CURLWS_CLOSE);
  }

 private:
  struct Outgoing {
    std::string data;
    unsigned int flags = 0;
  };

  static int OnTransfer(void* user, curl_off_t, curl_off_t, curl_off_t,
                        curl_off_t) {
    const auto* self = static_cast<CurlWebSocketImpl*>(user);
    return self->stopping_.load(std::memory_order_relaxed) ? 1 : 0;
  }

  void QueueSend(std::string data, unsigned int flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_state_ != WebSocket::OpenState)
      return;

    outgoing_.push_back(Outgoing{std::move(data), flags});
  }

  void QueueControl(std::string data, unsigned int flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoing_.push_back(Outgoing{std::move(data), flags});
  }

  void PushEvent(Persistent<NetworkEvent> event) {
    std::lock_guard<std::mutex> lock(mutex_);
    worker_queue_.push_back(std::move(event));
  }

  void Finish(int32_t state, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    ready_state_ = state;
    error_ = error;
  }

  bool DrainOutgoing(CURL* handle) {
    for (;;) {
      Outgoing item;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outgoing_.empty() || ready_state_ != WebSocket::OpenState)
          return true;

        item = std::move(outgoing_.front());
        outgoing_.pop_front();
      }

      std::size_t sent = 0;
      const CURLcode code =
          curl_ws_send(handle, item.data.data(), item.data.size(), &sent,
                       static_cast<curl_off_t>(item.data.size()), item.flags);
      if (code == CURLE_AGAIN) {
        std::lock_guard<std::mutex> lock(mutex_);
        outgoing_.push_front(std::move(item));
        return true;
      }
      if (code != CURLE_OK) {
        PushEvent(MakeEvent(NetworkEvent::Error));
        Finish(WebSocket::ClosedState, curl_easy_strerror(code));
        return false;
      }
    }
  }

  void HandleFrame(const std::string& chunk, const struct curl_ws_frame* meta) {
    if (meta->flags & CURLWS_PING) {
      QueueControl(chunk, CURLWS_PONG);
      return;
    }

    if (meta->flags & CURLWS_CLOSE) {
      int32_t code = 1005;
      std::string reason;
      if (chunk.size() >= 2) {
        code = (static_cast<unsigned char>(chunk[0]) << 8) |
               static_cast<unsigned char>(chunk[1]);
        reason = chunk.substr(2);
      }

      auto event = MakeEvent(NetworkEvent::Close);
      event->code_ = code;
      event->reason_ = reason;
      PushEvent(std::move(event));
      Finish(WebSocket::ClosedState, std::string());
      return;
    }

    if (meta->flags & CURLWS_CONT) {
      if (!in_message_)
        return;
    } else if (meta->flags & (CURLWS_TEXT | CURLWS_BINARY)) {
      if (!in_message_) {
        in_message_ = true;
        fragment_.clear();
        fragment_type_ = (meta->flags & CURLWS_TEXT) ? NetworkEvent::Text
                                                     : NetworkEvent::Binary;
      }
    }

    fragment_ += chunk;

    if (meta->bytesleft > 0)
      return;

    if (fragment_type_ == NetworkEvent::Text) {
      auto event = MakeEvent(NetworkEvent::Text);
      event->text_ = fragment_;
      event->data_ = fragment_;
      PushEvent(std::move(event));
    } else {
      auto event = MakeEvent(NetworkEvent::Binary);
      event->data_ = fragment_;
      PushEvent(std::move(event));
    }

    fragment_.clear();
    in_message_ = false;
  }

  void Run() {
    CURL* handle = curl_easy_init();
    if (!handle) {
      PushEvent(MakeEvent(NetworkEvent::Error));
      Finish(WebSocket::ClosedState, "curl_easy_init failed");
      return;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    // Stop must also interrupt a connecting socket, not just its receive loop.
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, OnTransfer);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);
    ConfigureTls(handle);

    curl_easy_setopt(handle, CURLOPT_WS_OPTIONS, (long)CURLWS_NOAUTOPONG);

    curl_slist* request_headers = nullptr;
    if (!protocols_.empty()) {
      std::string joined;
      for (std::size_t i = 0; i < protocols_.size(); ++i) {
        if (i)
          joined += ", ";
        joined += protocols_[i];
      }
      request_headers = curl_slist_append(
          request_headers, ("Sec-WebSocket-Protocol: " + joined).c_str());
      curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request_headers);
    }

    std::string header_block;
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, OnWrite);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &header_block);

    const CURLcode handshake = curl_easy_perform(handle);

    if (request_headers)
      curl_slist_free_all(request_headers);

    if (handshake != CURLE_OK) {
      PushEvent(MakeEvent(NetworkEvent::Error));
      Finish(WebSocket::ClosedState, curl_easy_strerror(handshake));
      curl_easy_cleanup(handle);
      return;
    }

    curl_socket_t sock = CURL_SOCKET_BAD;
    curl_easy_getinfo(handle, CURLINFO_ACTIVESOCKET, &sock);
    if (sock == CURL_SOCKET_BAD) {
      PushEvent(MakeEvent(NetworkEvent::Error));
      Finish(WebSocket::ClosedState, "the websocket has no active socket");
      curl_easy_cleanup(handle);
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      ready_state_ = WebSocket::OpenState;
    }
    PushEvent(MakeEvent(NetworkEvent::Open));

    std::vector<char> buffer(65536);

    for (;;) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_.load(std::memory_order_relaxed) ||
            ready_state_ == WebSocket::ClosedState)
          break;
      }

      if (!DrainOutgoing(handle))
        break;

      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(sock, &read_set);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 20000;

      const int ready = select(static_cast<int>(sock) + 1, &read_set, nullptr,
                               nullptr, &timeout);
      if (ready < 0)
        break;

      for (;;) {
        std::size_t received = 0;
        const struct curl_ws_frame* meta = nullptr;
        const CURLcode code = curl_ws_recv(handle, buffer.data(), buffer.size(),
                                           &received, &meta);
        if (code == CURLE_AGAIN)
          break;
        if (code != CURLE_OK) {
          PushEvent(MakeEvent(NetworkEvent::Error));
          Finish(WebSocket::ClosedState, curl_easy_strerror(code));
          break;
        }

        HandleFrame(std::string(buffer.data(), received), meta);

        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (stopping_.load(std::memory_order_relaxed) ||
              ready_state_ == WebSocket::ClosedState)
            break;
        }
      }

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_state_ == WebSocket::ClosedState)
          break;
      }
    }

    curl_easy_cleanup(handle);
  }

  NetworkBackend* backend_ = nullptr;
  std::string url_;
  std::vector<std::string> protocols_;

  mutable std::mutex mutex_;
  std::thread worker_;
  bool started_ = false;
  std::atomic<bool> stopping_{false};

  int32_t ready_state_ = WebSocket::Connecting;
  std::string error_;
  int32_t dropped_ = 0;
  int32_t max_events_ = 1024;

  std::string fragment_;
  int32_t fragment_type_ = NetworkEvent::Text;
  bool in_message_ = false;

  std::deque<Outgoing> outgoing_;
  std::deque<Persistent<NetworkEvent>> worker_queue_;
  std::deque<Persistent<NetworkEvent>> main_queue_;
};

class CurlFetchImpl final : public FetchImpl {
 public:
  CurlFetchImpl(NetworkBackend* backend,
                std::string url,
                std::string method,
                std::string body,
                std::vector<std::string> headers)
      : backend_(backend),
        url_(std::move(url)),
        method_(std::move(method)),
        body_(std::move(body)),
        headers_(std::move(headers)) {}

  ~CurlFetchImpl() override {
    if (backend_)
      backend_->Untrack(this);
    Abort();
  }

  void DetachBackend() {
    backend_ = nullptr;
    Abort();
  }

  void Start() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_ || aborted_.load(std::memory_order_relaxed))
      return;

    started_ = true;
    worker_ = std::thread(&CurlFetchImpl::Run, this);
  }

  void Abort() override {
    aborted_.store(true, std::memory_order_relaxed);
    Join();
  }

  void Poll(Fetch& self) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!finished_)
      return;

    self.body_ = std::move(result_body_);
    self.status_ = result_status_;
    self.status_text_ = std::move(result_status_text_);
    self.error_ = std::move(result_error_);
    self.headers_ = std::move(result_headers_);
    self.done_ = true;
    self.success_ =
        self.error_.empty() && result_status_ >= 200 && result_status_ < 300;
    self.progress_ = 1.0f;

    finished_ = false;
  }

 private:
  struct TransferData {
    CurlFetchImpl* self;
    std::string body;
    std::vector<std::string> headers;
  };

  static std::size_t OnWrite(char* data, std::size_t size, std::size_t count,
                             void* user) {
    auto* transfer = static_cast<TransferData*>(user);
    if (transfer->self->aborted_.load(std::memory_order_relaxed))
      return 0;
    const std::size_t total = size * count;
    transfer->body.append(data, total);
    return total;
  }

  static std::size_t OnHeader(char* data, std::size_t size, std::size_t count,
                              void* user) {
    auto* transfer = static_cast<TransferData*>(user);
    if (transfer->self->aborted_.load(std::memory_order_relaxed))
      return 0;
    const std::size_t total = size * count;
    transfer->headers.emplace_back(data, total);
    return total;
  }

  static int OnTransfer(void* user, curl_off_t, curl_off_t, curl_off_t,
                        curl_off_t) {
    const auto* self = static_cast<CurlFetchImpl*>(user);
    return self->aborted_.load(std::memory_order_relaxed) ? 1 : 0;
  }

  void Join() {
    if (worker_.joinable())
      worker_.join();
  }

  void Run() {
    CURL* handle = curl_easy_init();
    if (!handle) {
      FinishWithError("curl_easy_init failed");
      return;
    }

    TransferData transfer{this, {}, {}};
    curl_slist* request_headers = nullptr;

    curl_easy_setopt(handle, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "URGE");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, OnWrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, OnHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &transfer);
    // Returning nonzero cancels the actual transfer, including idle periods;
    // suppressing its eventual result alone would make Abort block in Join.
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, OnTransfer);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);
    ConfigureTls(handle);

    if (method_ != "GET" && method_ != "get") {
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method_.c_str());
      if (!body_.empty()) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body_.data());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(body_.size()));
      }
    }

    for (const auto& line : headers_)
      request_headers = curl_slist_append(request_headers, line.c_str());
    if (request_headers)
      curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request_headers);

    const CURLcode code = curl_easy_perform(handle);

    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);

    if (request_headers)
      curl_slist_free_all(request_headers);
    curl_easy_cleanup(handle);

    std::lock_guard<std::mutex> lock(mutex_);
    if (aborted_.load(std::memory_order_relaxed))
      return;

    result_status_ = static_cast<int32_t>(status);
    result_body_ = std::move(transfer.body);
    result_headers_ = std::move(transfer.headers);
    if (code != CURLE_OK)
      result_error_ = curl_easy_strerror(code);
    else
      result_status_text_ = StatusText(result_status_);
    finished_ = true;
  }

  void FinishWithError(const char* message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (aborted_.load(std::memory_order_relaxed))
      return;

    result_error_ = message;
    finished_ = true;
  }

  static std::string StatusText(int32_t status) {
    switch (status) {
      case 200:
        return "OK";
      case 201:
        return "Created";
      case 204:
        return "No Content";
      case 301:
        return "Moved Permanently";
      case 302:
        return "Found";
      case 304:
        return "Not Modified";
      case 400:
        return "Bad Request";
      case 401:
        return "Unauthorized";
      case 403:
        return "Forbidden";
      case 404:
        return "Not Found";
      case 405:
        return "Method Not Allowed";
      case 500:
        return "Internal Server Error";
      case 502:
        return "Bad Gateway";
      case 503:
        return "Service Unavailable";
      default:
        return std::string();
    }
  }

  NetworkBackend* backend_ = nullptr;
  std::string url_;
  std::string method_;
  std::string body_;
  std::vector<std::string> headers_;

  std::mutex mutex_;
  std::thread worker_;
  bool started_ = false;
  std::atomic<bool> aborted_{false};
  bool finished_ = false;

  std::string result_body_;
  int32_t result_status_ = 0;
  std::string result_status_text_;
  std::string result_error_;
  std::vector<std::string> result_headers_;
};

class CurlNetworkBackend final : public NetworkBackend {
 public:
  ~CurlNetworkBackend() override { GlobalCleanup(); }

  void GlobalInit() override {
    if (!initialized_)
      initialized_ = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
  }

  void GlobalCleanup() override {
    // Workers (notably Schannel transfers) must finish before global TLS
    // state is released. This also makes direct backend destruction safe.
    CloseAll();
    if (initialized_) {
      curl_global_cleanup();
      initialized_ = false;
    }
  }

  void Update() override {
    std::vector<CurlWebSocketImpl*> sockets;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets.assign(sockets_.begin(), sockets_.end());
    }

    for (auto* socket : sockets)
      socket->Swap();
  }

  void SetMaxEvents(int32_t max_events) override {
    std::lock_guard<std::mutex> lock(mutex_);
    max_events_ = max_events;
    for (auto* socket : sockets_)
      socket->SetMaxEvents(max_events_);
  }

  WebSocket* OpenWebSocket(const std::string& url,
                           const std::vector<std::string>& protocols) override {
    auto socket = MakeGarbageCollected<WebSocket>(url, protocols);
    auto impl = std::make_unique<CurlWebSocketImpl>(this, url, protocols);
    impl->SetMaxEvents(max_events_);
    CurlWebSocketImpl* raw = impl.get();
    socket->Attach(std::move(impl));

    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets_.push_back(raw);
    }
    raw->Start();
    return socket;
  }

  Fetch* OpenFetch(const std::string& url,
                   const std::string& method,
                   const std::string& body,
                   const std::vector<std::string>& headers) override {
    auto fetch = MakeGarbageCollected<Fetch>(url, method, body, headers);
    auto impl =
        std::make_unique<CurlFetchImpl>(this, url, method, body, headers);
    CurlFetchImpl* raw = impl.get();
    fetch->Attach(std::move(impl));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fetches_.push_back(raw);
    }
    return fetch;
  }

  void CloseAll() override {
    std::vector<CurlWebSocketImpl*> sockets;
    std::vector<CurlFetchImpl*> fetches;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets.swap(sockets_);
      fetches.swap(fetches_);
    }

    // No worker calls back into the registry. Detach on the main thread and
    // join outside its mutex; externally rooted wrappers may outlive us.
    for (auto* socket : sockets)
      socket->DetachBackend();
    for (auto* fetch : fetches)
      fetch->DetachBackend();
  }

  void Untrack(WebSocketImpl* impl) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* socket = static_cast<CurlWebSocketImpl*>(impl);
    sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket),
                   sockets_.end());
  }

  void Untrack(FetchImpl* impl) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* fetch = static_cast<CurlFetchImpl*>(impl);
    fetches_.erase(std::remove(fetches_.begin(), fetches_.end(), fetch),
                   fetches_.end());
  }

 private:
  std::mutex mutex_;
  // Non-owning registries; WebSocket/Fetch wrappers own their implementations.
  std::vector<CurlWebSocketImpl*> sockets_;
  std::vector<CurlFetchImpl*> fetches_;
  int32_t max_events_ = 1024;
  bool initialized_ = false;
};

}

std::unique_ptr<NetworkBackend> CreateNetworkBackend() {
  return std::make_unique<CurlNetworkBackend>();
}

}
