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

#include "core/network/network_backend.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
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

std::size_t OnHeader(char* data,
                     std::size_t size,
                     std::size_t count,
                     void* user) {
  auto* out = static_cast<std::vector<std::string>*>(user);
  const std::size_t total = size * count;
  std::string line(data, total);
  // Redirects and informational responses begin a new header block.
  if (line.compare(0, 5, "HTTP/") == 0)
    out->clear();
  out->push_back(std::move(line));
  return total;
}

RefPtr<NetworkEvent> MakeEvent(int32_t type) {
  auto event = MakeRefCounted<NetworkEvent>();
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
                    std::vector<std::string> protocols,
                    int32_t timeout)
      : backend_(backend),
        url_(std::move(url)),
        protocols_(std::move(protocols)),
        timeout_(timeout) {}

  ~CurlWebSocketImpl() override {
    if (backend_)
      backend_->Untrack(this);

    Stop();
  }

  void Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_)
      return;

    started_ = true;
    worker_ = std::thread(&CurlWebSocketImpl::Run, this);
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    if (worker_.joinable())
      worker_.join();
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

  void SetMaxEvents(int32_t max_events) override {
    std::lock_guard<std::mutex> lock(mutex_);
    max_events_ = max_events;
  }

  RefPtr<NetworkEvent> NextEvent() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (main_queue_.empty())
      return nullptr;

    auto event = std::move(main_queue_.front());
    main_queue_.pop_front();
    return event;
  }

  int32_t Pending() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int32_t>(main_queue_.size());
  }

  void Send(const std::string& data) override {
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
    std::size_t offset = 0;
  };

  void QueueSend(std::string data, unsigned int flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_state_ != WebSocket::OpenState)
      return;

    outgoing_.push_back(Outgoing{std::move(data), flags});
    if (flags & CURLWS_CLOSE)
      ready_state_ = WebSocket::Closing;
  }

  void QueueControl(std::string data, unsigned int flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoing_.push_back(Outgoing{std::move(data), flags});
  }

  void PushEvent(RefPtr<NetworkEvent> event) {
    std::lock_guard<std::mutex> lock(mutex_);
    worker_queue_.push_back(std::move(event));
  }

  void Finish(int32_t state, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    ready_state_ = state;
    error_ = error;
  }

  void Fail(const std::string& message) {
    auto event = MakeEvent(NetworkEvent::Error);
    event->message_ = message;
    Finish(WebSocket::ClosedState, message);
    PushEvent(std::move(event));
  }

  bool DrainOutgoing(CURL* handle) {
    for (;;) {
      Outgoing item;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outgoing_.empty() || ready_state_ == WebSocket::ClosedState)
          return true;

        item = std::move(outgoing_.front());
        outgoing_.pop_front();
      }

      std::size_t sent = 0;
      const CURLcode code =
          curl_ws_send(handle, item.data.data() + item.offset,
                       item.data.size() - item.offset, &sent, 0, item.flags);
      if (code != CURLE_OK && code != CURLE_AGAIN) {
        Fail(curl_easy_strerror(code));
        return false;
      }
      item.offset += sent;
      if (code == CURLE_AGAIN || item.offset < item.data.size()) {
        std::lock_guard<std::mutex> lock(mutex_);
        outgoing_.push_front(std::move(item));
        return true;
      }
      if (item.flags & CURLWS_CLOSE)
        close_sent_ = true;
    }
  }

  void HandleFrame(const std::string& chunk, const struct curl_ws_frame* meta) {
    if (meta->flags & (CURLWS_PING | CURLWS_PONG | CURLWS_CLOSE)) {
      if (meta->offset == 0)
        control_.clear();
      control_ += chunk;
      if (meta->bytesleft > 0)
        return;
      if (meta->flags & CURLWS_PING) {
        QueueControl(control_, CURLWS_PONG);
      } else if (meta->flags & CURLWS_CLOSE) {
        auto event = MakeEvent(NetworkEvent::Close);
        event->code_ = 1005;
        if (control_.size() >= 2) {
          event->code_ = (static_cast<unsigned char>(control_[0]) << 8) |
                         static_cast<unsigned char>(control_[1]);
          event->reason_ = control_.substr(2);
        }
        // Reply to a server-initiated close before releasing the connection.
        if (!close_sent_)
          QueueControl(control_, CURLWS_CLOSE);
        received_close_ = std::move(event);
        std::lock_guard<std::mutex> lock(mutex_);
        ready_state_ = WebSocket::Closing;
      }
      return;  // Pongs are control frames, never Ruby data events.
    }

    if (!(meta->flags & (CURLWS_TEXT | CURLWS_BINARY)))
      return;
    if (!in_message_) {
      in_message_ = true;
      fragment_.clear();
    }
    fragment_ += chunk;
    // bytesleft describes the current frame; CONT means another frame follows.
    if (meta->bytesleft > 0 || (meta->flags & CURLWS_CONT))
      return;

    auto event = MakeEvent(NetworkEvent::Message);
    event->data_ = std::move(fragment_);
    PushEvent(std::move(event));
    in_message_ = false;
  }

  void Run() {
    CURL* handle = curl_easy_init();
    if (!handle) {
      Fail("curl_easy_init failed");
      return;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url_.c_str());
    connect_deadline_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeout_);
    curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, static_cast<long>(timeout_));
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT,
                     static_cast<long>(timeout_));
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(
        handle, CURLOPT_XFERINFOFUNCTION,
        +[](void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
          auto* self = static_cast<CurlWebSocketImpl*>(user);
          std::lock_guard<std::mutex> lock(self->mutex_);
          return self->stopping_ ||
                         (self->ready_state_ == WebSocket::Connecting &&
                          std::chrono::steady_clock::now() >=
                              self->connect_deadline_)
                     ? 1
                     : 0;
        });
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
      Fail(curl_easy_strerror(handshake));
      curl_easy_cleanup(handle);
      return;
    }

    curl_socket_t sock = CURL_SOCKET_BAD;
    curl_easy_getinfo(handle, CURLINFO_ACTIVESOCKET, &sock);
    if (sock == CURL_SOCKET_BAD) {
      Fail("the websocket has no active socket");
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
        if (stopping_ || ready_state_ == WebSocket::ClosedState)
          break;
      }

      if (!DrainOutgoing(handle))
        break;
      if (received_close_) {
        bool drained = false;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          drained = outgoing_.empty();
        }
        if (drained) {
          Finish(WebSocket::ClosedState, std::string());
          PushEvent(std::move(received_close_));
          break;
        }
        continue;
      }

      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(sock, &read_set);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 20000;

      const int ready = select(static_cast<int>(sock) + 1, &read_set, nullptr,
                               nullptr, &timeout);
      if (ready < 0) {
        Fail("websocket select failed");
        break;
      }

      for (;;) {
        std::size_t received = 0;
        const struct curl_ws_frame* meta = nullptr;
        const CURLcode code = curl_ws_recv(handle, buffer.data(), buffer.size(),
                                           &received, &meta);
        if (code == CURLE_AGAIN)
          break;
        if (code != CURLE_OK) {
          Fail(curl_easy_strerror(code));
          break;
        }

        HandleFrame(std::string(buffer.data(), received), meta);
        if (received_close_)
          break;

        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (ready_state_ == WebSocket::ClosedState)
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
  int32_t timeout_ = 30;
  std::chrono::steady_clock::time_point connect_deadline_;

  mutable std::mutex mutex_;
  std::thread worker_;
  bool started_ = false;
  bool stopping_ = false;

  int32_t ready_state_ = WebSocket::Connecting;
  std::string error_;
  int32_t dropped_ = 0;
  int32_t max_events_ = 1024;

  std::string fragment_;
  bool in_message_ = false;
  std::string control_;
  bool close_sent_ = false;
  RefPtr<NetworkEvent> received_close_;

  std::deque<Outgoing> outgoing_;
  std::deque<RefPtr<NetworkEvent>> worker_queue_;
  std::deque<RefPtr<NetworkEvent>> main_queue_;
};

class CurlFetchImpl final : public FetchImpl {
 public:
  CurlFetchImpl(std::string url,
                std::string method,
                std::string body,
                std::vector<std::string> headers,
                int32_t timeout)
      : url_(std::move(url)),
        method_(std::move(method)),
        body_(std::move(body)),
        headers_(std::move(headers)),
        timeout_(timeout) {}

  ~CurlFetchImpl() override {
    aborted_.store(true);
    Join();
  }

  void Start() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_)
      return;

    started_ = true;
    worker_ = std::thread(&CurlFetchImpl::Run, this);
  }

  void Abort() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (completed_)
      return;
    aborted_.store(true);
    started_ = true;  // Aborting an idle request must not allow a later start.
    result_error_ = "the request was aborted";
    completed_ = finished_ = true;
    // The worker observes aborted_ in its progress callback; never block Ruby.
  }

  void Poll(Fetch& self) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!self.done_)
      self.progress_ = progress_.load();
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

    std::string body;
    std::vector<std::string> header_lines;
    curl_slist* request_headers = nullptr;

    curl_easy_setopt(handle, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "URGE");
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, static_cast<long>(timeout_));
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(
        handle, CURLOPT_XFERINFOFUNCTION,
        +[](void* user, curl_off_t total, curl_off_t received, curl_off_t,
            curl_off_t) -> int {
          auto* self = static_cast<CurlFetchImpl*>(user);
          if (total > 0)
            self->progress_.store(static_cast<float>(received) /
                                  static_cast<float>(total));
          return self->aborted_.load() ? 1 : 0;
        });
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, OnWrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, OnHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &header_lines);
    ConfigureTls(handle);

    if (method_ == "HEAD" || method_ == "head") {
      curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
    } else if (method_ != "GET" && method_ != "get") {
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
    if (aborted_)
      return;

    result_status_ = static_cast<int32_t>(status);
    result_body_ = std::move(body);
    result_headers_ = std::move(header_lines);
    if (code != CURLE_OK)
      result_error_ = curl_easy_strerror(code);
    else if (!result_headers_.empty()) {
      const auto& status_line = result_headers_.front();
      const auto first = status_line.find(' ');
      const auto second = first == std::string::npos
                              ? std::string::npos
                              : status_line.find(' ', first + 1);
      if (second != std::string::npos) {
        result_status_text_ = status_line.substr(second + 1);
        const auto end = result_status_text_.find_last_not_of("\r\n");
        result_status_text_.resize(end == std::string::npos ? 0 : end + 1);
      }
    }
    completed_ = finished_ = true;
  }

  void FinishWithError(const char* message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (aborted_)
      return;

    result_error_ = message;
    completed_ = finished_ = true;
  }

  std::string url_;
  std::string method_;
  std::string body_;
  std::vector<std::string> headers_;
  int32_t timeout_ = 30;

  std::mutex mutex_;
  std::thread worker_;
  bool started_ = false;
  std::atomic_bool aborted_ = false;
  std::atomic<float> progress_ = 0.0f;
  bool completed_ = false;
  bool finished_ = false;

  std::string result_body_;
  int32_t result_status_ = 0;
  std::string result_status_text_;
  std::string result_error_;
  std::vector<std::string> result_headers_;
};

class CurlNetworkBackend final : public NetworkBackend {
 public:
  void GlobalInit() override { curl_global_init(CURL_GLOBAL_DEFAULT); }

  void GlobalCleanup() override { curl_global_cleanup(); }

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

  std::unique_ptr<WebSocketImpl> CreateWebSocketImpl(
      const std::string& url,
      const std::vector<std::string>& protocols,
      int32_t timeout) override {
    auto impl =
        std::make_unique<CurlWebSocketImpl>(this, url, protocols, timeout);
    impl->SetMaxEvents(max_events_);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets_.push_back(impl.get());
    }
    impl->Start();
    return impl;
  }

  std::unique_ptr<FetchImpl> CreateFetchImpl(
      const std::string& url,
      const std::string& method,
      const std::string& body,
      const std::vector<std::string>& headers,
      int32_t timeout) override {
    return std::make_unique<CurlFetchImpl>(url, method, body, headers, timeout);
  }

  void CloseAll() override {
    std::vector<CurlWebSocketImpl*> sockets;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sockets.swap(sockets_);
    }

    for (auto* socket : sockets)
      socket->Stop();
  }

  void Untrack(WebSocketImpl* impl) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* socket = static_cast<CurlWebSocketImpl*>(impl);
    sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket),
                   sockets_.end());
  }

 private:
  std::mutex mutex_;
  std::vector<CurlWebSocketImpl*> sockets_;
  int32_t max_events_ = 1024;
};

}  // namespace

std::unique_ptr<NetworkBackend> CreateNetworkBackend() {
  return std::make_unique<CurlNetworkBackend>();
}

}  // namespace urge
