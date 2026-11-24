// Exercise the real Web backend without pulling in the graphics/Ruby engine.
#include "core/network/network.h"

#include <emscripten/emscripten.h>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void Assert(bool value, const char* message) {
  if (!value)
    throw std::runtime_error(message);
}

void Wait(const std::function<bool()>& ready) {
  const double deadline = emscripten_get_now() + 5000;
  while (!ready()) {
    Assert(emscripten_get_now() < deadline, "network wait timed out");
    urge::Network::Get().Update();
    emscripten_sleep(10);
  }
}

urge::RefPtr<urge::NetworkEvent> Next(urge::WebSocket& socket) {
  urge::RefPtr<urge::NetworkEvent> event;
  Wait([&] { event = socket.NextEvent(); return !!event; });
  return event;
}

std::string Bytes(size_t size) {
  std::string result(size, '\0');
  for (size_t i = 0; i < size; ++i)
    result[i] = static_cast<char>(i % 256);
  return result;
}

int failures = 0;

void Check(const char* name, const std::function<void()>& test) {
  try {
    test();
    std::cout << "PASS " << name << std::endl;
  } catch (const std::exception& error) {
    ++failures;
    std::cout << "FAIL " << name << ": " << error.what() << std::endl;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Assert(argc == 2, "expected fixture URL");
  const std::string base = argv[1];
  const std::string ws_base = "ws" + base.substr(4);
  const std::string binary = Bytes(256 * 1024);
  const std::string text = std::string("\xef\xbb\xbf") +
                           std::string("echo\0\xe4\xb8\xad\xe6\x96\x87\0", 12);
  urge::Network::Reset(new urge::Network());

  Check("Fetch GET, chunked and non-2xx preserve arbitrary binary bytes", [&] {
    for (const auto* path : {"/binary", "/binary-chunked", "/binary-status"}) {
      urge::Fetch fetch(base + path);
      fetch.Start();
      Wait([&] { return fetch.IsDone(); });
      const int status = std::string(path) == "/binary-status" ? 418 : 200;
      Assert(fetch.GetStatus() == status, "response status mismatch");
      Assert(fetch.IsSuccess() == (status == 200), "response success mismatch");
      Assert(fetch.GetBody() == binary, "binary response bytes changed");
    }
  });

  Check("Fetch POST preserves all byte values and embedded NULs", [&] {
    urge::Fetch fetch(base + "/echo", "POST", binary,
                      {"Content-Type: application/octet-stream"});
    fetch.Start();
    Wait([&] { return fetch.IsDone(); });
    Assert(fetch.IsSuccess() && fetch.GetStatus() == 201, "POST failed");
    Assert(fetch.GetBody() == binary, "POST bytes changed");
  });

  Check("Fetch empty body", [&] {
    urge::Fetch fetch(base + "/empty");
    fetch.Start();
    Wait([&] { return fetch.IsDone(); });
    Assert(fetch.IsSuccess() && fetch.GetBody().empty(), "empty response changed");
  });

  Check("WebSocket binary frame length boundaries and empty payload", [&] {
    urge::WebSocket socket(ws_base + "/binary-only");
    Assert(Next(socket)->GetType() == urge::NetworkEvent::Open, "missing open");
    for (size_t size : {0, 125, 126, 65535, 65536, 262144}) {
      const auto bytes = Bytes(size);
      socket.Send(bytes);
      const auto event = Next(socket);
      const auto received = event->GetData();
      Assert(event->GetType() == urge::NetworkEvent::Message, "wrong message type");
      Assert(received == bytes,
             ("WebSocket bytes changed at length " + std::to_string(size) +
              ", received " + std::to_string(received.size())).c_str());
    }
    socket.Close();
  });

  Check("WebSocket sends UTF-8, BOM and NUL bytes as binary without decoding", [&] {
    urge::WebSocket socket(ws_base + "/binary-only");
    Assert(Next(socket)->GetType() == urge::NetworkEvent::Open, "missing open");
    for (const auto& value : {text, text + std::string(64, 'x')}) {
      socket.Send(value);
      const auto event = Next(socket);
      Assert(event->GetType() == urge::NetworkEvent::Message && event->GetData() == value,
             "UTF-8-like bytes changed");
    }
    socket.Close();
  });

  Check("WebSocket wire text and binary frames share a raw message API", [&] {
    urge::WebSocket socket(ws_base + "/mixed");
    Assert(Next(socket)->GetType() == urge::NetworkEvent::Open, "missing open");
    for (const auto& value : {text, text, std::string(), std::string()}) {
      const auto event = Next(socket);
      Assert(event->GetType() == urge::NetworkEvent::Message && event->GetData() == value,
             "wire frame type changed the raw message payload");
    }
    socket.Close();
  });

  Check("WebSocket fragmented binary with interleaved ping", [&] {
    urge::WebSocket socket(ws_base + "/binary-fragments");
    Assert(Next(socket)->GetType() == urge::NetworkEvent::Open, "missing open");
    const auto event = Next(socket);
    Assert(event->GetType() == urge::NetworkEvent::Message && event->GetData() == binary,
           "fragmented binary bytes changed");
    Assert(Next(socket)->GetData() == "server-pong-seen", "browser did not pong");
    socket.Close();
  });

  Check("WebSocket fragmented text with interleaved ping is also raw data", [&] {
    urge::WebSocket socket(ws_base + "/fragments");
    Assert(Next(socket)->GetType() == urge::NetworkEvent::Open, "missing open");
    const auto event = Next(socket);
    Assert(event->GetType() == urge::NetworkEvent::Message &&
           event->GetData() == "fragment-\xe4\xb8\xad\xe6\x96\x87",
           "fragmented wire text bytes changed");
    Assert(Next(socket)->GetData() == "server-pong-seen", "browser did not pong");
    socket.Close();
  });

  urge::Network::Reset(nullptr);
  if (!failures)
    std::cout << "ALL_OK" << std::endl;
  return failures ? 1 : 0;
}
