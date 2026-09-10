#pragma once

#include <Arduino.h>

#include <functional>
#include <string>
#include <vector>

enum HTTPMethod : uint8_t { HTTP_ANY, HTTP_GET, HTTP_POST };

struct FakeWebServerState {
  int liveInstances = 0;
  uint32_t beginCalls = 0;
  uint32_t stopCalls = 0;
  std::vector<std::string> routes;
};

extern FakeWebServerState FakeWebServer;

class WebServer {
public:
  using Handler = std::function<void()>;

  explicit WebServer(uint16_t) {
    ++FakeWebServer.liveInstances;
    FakeWebServer.routes.clear();
  }

  ~WebServer() { --FakeWebServer.liveInstances; }

  void on(const char* path, HTTPMethod, Handler) {
    FakeWebServer.routes.emplace_back(path ? path : "");
  }

  void onNotFound(Handler) {}
  void begin() { ++FakeWebServer.beginCalls; }
  void stop() { ++FakeWebServer.stopCalls; }
  void handleClient() {}
  String arg(const char*) const { return String(); }
  void send_P(int, const char*, const char*) {}
  void send(int, const char*, const char*) {}
  void send(int, const char*, const String&) {}
  void sendHeader(const char*, const char*, bool = false) {}
  void sendHeader(const char*, const String&, bool = false) {}
};
