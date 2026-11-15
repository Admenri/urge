// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Use local servers that accept but stall WebSocket/HTTP responses.
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "core/common/garbage_collector.h"
#include "core/common/persistent.h"
#include "core/network/network.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: network_roots <local-ws-url> <local-http-url>\n");
    return 2;
  }
  using namespace urge;
  using Clock = std::chrono::steady_clock;
  auto& gc = GarbageCollector::Get();
  gc.ClearHost();
  const auto baseline = gc.ObjectCount();
  Persistent<WebSocket> socket;
  Persistent<Fetch> active;
  Persistent<Fetch> idle;
  WeakMember<WebSocket> weak_socket;
  WeakMember<Fetch> weak_active;
  WeakMember<Fetch> weak_idle;
  {
    Network network;
    ExceptionState exception;
    socket = network.OpenWebSocket(exception, argv[1]);
    active = network.OpenFetch(exception, argv[2]);
    idle = network.OpenFetch(exception, argv[2]);
    assert(!exception.HadException() && socket && active && idle);
    weak_socket = socket.get();
    weak_active = active.get();
    weak_idle = idle.get();
    active->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto begin = Clock::now();
    network.CloseAll();
    const auto seconds = std::chrono::duration<double>(Clock::now() - begin).count();
    std::printf("CloseAll seconds: %.3f\n", seconds);
    assert(seconds < 5.0);  // Local responsive DNS; not a global resolver bound.
    assert(socket->GetReadyState() == WebSocket::ClosedState);
    network.CloseAll();
    gc.Collect();  // Workers stopped; external roots survive with the backend alive.
    assert(weak_socket && weak_active && weak_idle);
  }
  gc.Collect();  // The same roots must outlive the Network service itself.
  assert(weak_socket && weak_active && weak_idle);
  assert(socket->GetReadyState() == WebSocket::ClosedState);
  socket->SendText("ignored after shutdown");
  socket->Close();
  idle->Start();  // Detached backends must not restart after global cleanup.
  idle->Abort();
  active->Abort();
  socket.reset();
  active.reset();
  idle.reset();
  gc.Collect();  // Formerly called Untrack through an already deleted backend.
  assert(!weak_socket && !weak_active && !weak_idle);
  gc.Collect();  // Leaf event task roots were released by socket preparation.
  {
    Network network;
    ExceptionState exception;
    socket = network.OpenWebSocket(exception, argv[1]);
    active = network.OpenFetch(exception, argv[2]);
    assert(!exception.HadException() && socket && active);
    weak_socket = socket.get();
    weak_active = active.get();
    active->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Destructor-driven CloseAll/GlobalCleanup, while these native roots live.
  }
  gc.Collect();
  assert(weak_socket && weak_active);
  assert(socket->GetReadyState() == WebSocket::ClosedState);
  socket.reset();
  active.reset();
  gc.Collect();
  assert(!weak_socket && !weak_active);
  gc.Collect();
  assert(gc.ObjectCount() == baseline);
  std::puts("network native-root regressions passed");
}
