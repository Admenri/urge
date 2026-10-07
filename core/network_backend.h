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

#include <memory>
#include <string>
#include <vector>

#include "core/network.h"

namespace urge {

class WebSocketImpl {
 public:
  virtual ~WebSocketImpl() = default;

  WebSocketImpl(const WebSocketImpl&) = delete;
  WebSocketImpl& operator=(const WebSocketImpl&) = delete;

  virtual void Poll(WebSocket& self) = 0;
  virtual void Swap() = 0;
  virtual void SetMaxEvents(int32_t max_events) = 0;

  virtual RefPtr<NetworkEvent> NextEvent() = 0;
  virtual int32_t Pending() = 0;

  virtual void SendText(const std::string& data) = 0;
  virtual void SendBinary(const std::string& data) = 0;
  virtual void SendPing(const std::string& data) = 0;
  virtual void Close(int32_t code, const std::string& reason) = 0;

 protected:
  WebSocketImpl() = default;
};

class FetchImpl {
 public:
  virtual ~FetchImpl() = default;

  FetchImpl(const FetchImpl&) = delete;
  FetchImpl& operator=(const FetchImpl&) = delete;

  virtual void Poll(Fetch& self) = 0;
  virtual void Start() = 0;
  virtual void Abort() = 0;

 protected:
  FetchImpl() = default;
};

class NetworkBackend {
 public:
  virtual ~NetworkBackend() = default;

  NetworkBackend(const NetworkBackend&) = delete;
  NetworkBackend& operator=(const NetworkBackend&) = delete;

  virtual void GlobalInit() = 0;
  virtual void GlobalCleanup() = 0;

  /* The pump behind `Network.update`.  Every live socket swaps its worker
     queue into the queue the script reads, which is what fixes the visibility
     of an event to the update call after the one that saw it arrive. */
  virtual void Update() = 0;

  virtual void SetMaxEvents(int32_t max_events) = 0;

  virtual RefPtr<WebSocket> OpenWebSocket(
      const std::string& url, const std::vector<std::string>& protocols) = 0;
  virtual RefPtr<Fetch> OpenFetch(const std::string& url,
                                  const std::string& method,
                                  const std::string& body,
                                  const std::vector<std::string>& headers) = 0;

  virtual void CloseAll() = 0;

  virtual void Untrack(WebSocketImpl* impl) = 0;

 protected:
  NetworkBackend() = default;
};

std::unique_ptr<NetworkBackend> CreateNetworkBackend();

}  // namespace urge
