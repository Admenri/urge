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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/definition.h"
#include "core/object.h"
#include "core/refptr.h"

namespace urge {

class NetworkBackend;
class WebSocketImpl;
class FetchImpl;

URGE_BINDING()
class NetworkEvent : public Object {
 public:
  enum Type {
    Open = 0,
    Text = 1,
    Binary = 2,
    Close = 3,
    Error = 4,
  };

  URGE_BINDING()
  NetworkEvent();
  URGE_BINDING()
  ~NetworkEvent() override;

  URGE_BINDING(Name : "type")
  int32_t GetType();
  URGE_BINDING(Name : "text")
  std::string GetText();
  URGE_BINDING(Name : "code")
  int32_t GetCode();
  URGE_BINDING(Name : "reason")
  std::string GetReason();
  URGE_BINDING(Name : "message")
  std::string GetErrorMessage();

  const std::string& data() const { return data_; }

  int32_t type_ = Open;
  std::string data_;
  std::string text_;
  int32_t code_ = 0;
  std::string reason_;
  std::string message_;
};

URGE_BINDING()
class WebSocket : public Object {
 public:
  enum ReadyState {
    Connecting = 0,
    OpenState = 1,
    Closing = 2,
    ClosedState = 3,
  };

  URGE_BINDING()
  WebSocket(std::string url, std::vector<std::string> protocols = {});
  URGE_BINDING()
  ~WebSocket() override;

  URGE_BINDING(Name : "url")
  std::string GetUrl();
  URGE_BINDING(Name : "ready_state")
  int32_t GetReadyState();
  URGE_BINDING(Name : "error")
  std::string GetError();
  URGE_BINDING(Name : "dropped")
  int32_t GetDropped();

  URGE_BINDING()
  void SendText(std::string data);
  URGE_BINDING()
  void SendBinary(std::string data);
  URGE_BINDING()
  void SendPing(std::string data = "");
  URGE_BINDING()
  void Close(int32_t code = 1000, std::string reason = "");

  URGE_BINDING()
  RefPtr<NetworkEvent> NextEvent();
  URGE_BINDING(Name : "pending")
  int32_t GetPending();

  void Attach(std::unique_ptr<WebSocketImpl> impl);

  int32_t ready_state_ = Connecting;
  std::string error_;
  int32_t dropped_ = 0;

 private:
  std::string url_;
  std::vector<std::string> protocols_;
  std::unique_ptr<WebSocketImpl> impl_;
};

URGE_BINDING()
class Fetch : public Object {
 public:
  URGE_BINDING()
  Fetch(std::string url, std::string method = "GET", std::string body = "",
        std::vector<std::string> headers = {});
  URGE_BINDING()
  ~Fetch() override;

  URGE_BINDING()
  void Start();
  URGE_BINDING()
  void Abort();

  URGE_BINDING(Name : "done?")
  bool IsDone();
  URGE_BINDING(Name : "success?")
  bool IsSuccess();

  URGE_BINDING(Name : "status")
  int32_t GetStatus();
  URGE_BINDING(Name : "status_text")
  std::string GetStatusText();
  URGE_BINDING(Name : "text")
  std::string GetText();
  URGE_BINDING(Name : "error")
  std::string GetError();
  URGE_BINDING(Name : "progress")
  float GetProgress();

  URGE_BINDING(Name : "header")
  std::string GetHeader(std::string name);
  URGE_BINDING(Name : "headers")
  std::vector<std::string> GetHeaders();

  const std::string& body() const { return body_; }

  void Attach(std::unique_ptr<FetchImpl> impl);

  std::string body_;
  int32_t status_ = 0;
  std::string status_text_;
  std::string error_;
  float progress_ = 0.0f;
  bool done_ = false;
  bool success_ = false;
  std::vector<std::string> headers_;

 private:
  std::string url_;
  std::string method_;
  std::string request_body_;
  std::vector<std::string> request_headers_;
  std::unique_ptr<FetchImpl> impl_;
};

URGE_BINDING()
class Network : public Singleton<Network> {
 public:
  Network();
  ~Network();

  URGE_BINDING()
  void Update();
  URGE_BINDING(Name : "websocket")
  RefPtr<WebSocket> OpenWebSocket(std::string url,
                                  std::vector<std::string> protocols = {});
  URGE_BINDING(Name : "fetch")
  RefPtr<Fetch> OpenFetch(std::string url, std::string method = "GET",
                          std::string body = "",
                          std::vector<std::string> headers = {});

  URGE_BINDING()
  ATTR(int32_t, Timeout);
  URGE_BINDING()
  ATTR(int32_t, MaxEvents);

  void CloseAll();

 private:
  std::unique_ptr<NetworkBackend> backend_;
  int32_t timeout_ = 30;
  int32_t max_events_ = 1024;
};

}  // namespace urge
