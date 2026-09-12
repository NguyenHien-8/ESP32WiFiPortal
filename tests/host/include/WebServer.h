#pragma once

#include <Arduino.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

enum HTTPMethod : uint8_t { HTTP_ANY, HTTP_GET, HTTP_POST };

struct FakeWebServerState {
  int liveInstances = 0;
  uint32_t beginCalls = 0;
  uint32_t stopCalls = 0;
  std::vector<std::string> routes;
  std::map<std::string, std::function<void()>> handlers;
  std::map<std::string, HTTPMethod> methods;
  std::map<std::string, std::string> args;
  std::map<std::string, std::string> responseHeaders;
  int lastStatus = 0;
  std::string lastContentType;
  std::string lastBody;
};

extern FakeWebServerState FakeWebServer;

class WebServer {
public:
  using Handler = std::function<void()>;

  explicit WebServer(uint16_t) {
    ++FakeWebServer.liveInstances;
    FakeWebServer.routes.clear();
    FakeWebServer.handlers.clear();
    FakeWebServer.methods.clear();
  }

  ~WebServer() { --FakeWebServer.liveInstances; }

  void on(const char* path, HTTPMethod method, Handler handler) {
    const std::string route(path ? path : "");
    FakeWebServer.routes.emplace_back(route);
    FakeWebServer.handlers[route] = handler;
    FakeWebServer.methods[route] = method;
  }

  void onNotFound(Handler) {}
  void begin() { ++FakeWebServer.beginCalls; }
  void stop() { ++FakeWebServer.stopCalls; }
  void handleClient() {}
  String arg(const char* name) const {
    const auto found = FakeWebServer.args.find(name ? name : "");
    return found == FakeWebServer.args.end() ? String()
                                               : String(found->second.c_str());
  }
  void send_P(int status, const char* contentType, const char* body) {
    FakeWebServer.lastStatus = status;
    FakeWebServer.lastContentType = contentType ? contentType : "";
    FakeWebServer.lastBody = body ? body : "";
  }
  void send(int status, const char* contentType, const char* body) {
    FakeWebServer.lastStatus = status;
    FakeWebServer.lastContentType = contentType ? contentType : "";
    FakeWebServer.lastBody = body ? body : "";
  }
  void send(int status, const char* contentType, const String& body) {
    FakeWebServer.lastStatus = status;
    FakeWebServer.lastContentType = contentType ? contentType : "";
    FakeWebServer.lastBody = body.c_str();
  }
  void sendHeader(const char* name, const char* value, bool = false) {
    FakeWebServer.responseHeaders[name ? name : ""] = value ? value : "";
  }
  void sendHeader(const char* name, const String& value, bool = false) {
    FakeWebServer.responseHeaders[name ? name : ""] = value.c_str();
  }
};
