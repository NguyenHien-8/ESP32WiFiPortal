#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

bool hasRoute(const char* route) {
  return std::find(FakeWebServer.routes.begin(), FakeWebServer.routes.end(),
                   std::string(route)) != FakeWebServer.routes.end();
}

void expectMethod(const char* route, HTTPMethod method) {
  assert(FakeWebServer.methods.at(route) == method);
}

void expectContains(const std::string& value, const char* expected) {
  assert(value.find(expected) != std::string::npos);
}

void expectFullyStopped(const ESP32WiFiPortal& portal) {
  assert(!portal.isPortalActive());
  assert(!FakeWiFi.apActive);
  assert(!FakeDNS.active);
  assert(FakeWebServer.liveInstances == 0);
}

}  // namespace

int main() {
  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);

    assert(portal.portalIP() == IPAddress(192, 168, 4, 1));
    assert(portal.startConfigPortalAsync("Default-Portal", nullptr, 0));
    assert(portal.isPortalActive());
    assert(FakeWiFi.runtimeIP == IPAddress(192, 168, 4, 1));
    assert(FakeDNS.address == IPAddress(192, 168, 4, 1));
    assert(std::string(FakeDNS.domain.c_str()) == "*");
    assert(std::string(ESP32WiFiPortalTestAccess::redirectURL(portal).c_str()) ==
           "http://192.168.4.1/");
    assert(hasRoute("/"));
    assert(hasRoute("/wifi"));
    assert(hasRoute("/scan"));
    assert(hasRoute("/save"));
    assert(hasRoute("/status"));
    assert(hasRoute("/properties"));
    assert(hasRoute("/reset"));
    expectMethod("/properties", HTTP_GET);
    expectMethod("/reset", HTTP_POST);
    assert(hasRoute("/generate_204"));
    assert(hasRoute("/gen_204"));
    assert(hasRoute("/hotspot-detect.html"));
    assert(hasRoute("/library/test/success.html"));
    assert(hasRoute("/connecttest.txt"));
    assert(hasRoute("/ncsi.txt"));
    assert(hasRoute("/fwlink"));

    FakeWebServer.handlers.at("/properties")();
    assert(FakeWebServer.lastStatus == 200);
    assert(FakeWebServer.lastContentType ==
           "application/json; charset=utf-8");
    assert(FakeWebServer.responseHeaders["Cache-Control"] == "no-store");
    std::string properties = FakeWebServer.lastBody;
    expectContains(properties, "\"ssid\":\"Default-Portal\"");
    expectContains(properties,
                   "\"softap\":{\"ip\":\"192.168.4.1\",\"gateway\":\"192.168.4.1\",\"subnet\":\"255.255.255.0\"}");
    expectContains(properties,
                   "\"sta\":{\"connected\":false,\"ip\":\"\",\"gateway\":\"\",\"subnet\":\"\"}");
    expectContains(properties,
                   "\"mac\":{\"ap\":\"AA:BB:CC:DD:EE:01\",\"sta\":\"AA:BB:CC:DD:EE:02\"}");
    expectContains(properties, "\"chipId\":\"0123456789ABCDEF\"");
    expectContains(properties, "\"cpuMHz\":240");
    expectContains(properties, "\"flashSize\":4194304");
    expectContains(properties, "\"flashSpeed\":80000000");
    expectContains(properties, "\"freeHeap\":131072");
    assert(properties.find("password") == std::string::npos);

    FakeWiFi.status = WL_CONNECTED;
    FakeWiFi.staIP = IPAddress(10, 20, 30, 40);
    FakeWiFi.staGateway = IPAddress(10, 20, 30, 1);
    FakeWiFi.staSubnet = IPAddress(255, 255, 255, 0);
    FakeESP.freeHeap = 130048;
    FakeWebServer.handlers.at("/properties")();
    properties = FakeWebServer.lastBody;
    expectContains(properties,
                   "\"sta\":{\"connected\":true,\"ip\":\"10.20.30.40\",\"gateway\":\"10.20.30.1\",\"subnet\":\"255.255.255.0\"}");
    expectContains(properties, "\"freeHeap\":130048");
    FakeWiFi.status = WL_DISCONNECTED;

    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Keep Router"), String("keep-password")));
    FakeWebServer.args["ssid"] = "Pending Router";
    FakeWebServer.args["password"] = "pending-password";
    FakeWebServer.handlers.at("/save")();
    assert(FakeWebServer.lastStatus == 200);
    FakeWebServer.handlers.at("/properties")();
    properties = FakeWebServer.lastBody;
    assert(properties.find("Keep Router") == std::string::npos);
    assert(properties.find("keep-password") == std::string::npos);
    assert(properties.find("Pending Router") == std::string::npos);
    assert(properties.find("pending-password") == std::string::npos);
    const auto credentialsBeforeReset = FakePreferences.bytes;
    const uint32_t removeCallsBeforeReset = FakePreferences.removeCalls;
    FakeMillis = 100;
    FakeWebServer.handlers.at("/reset")();
    assert(FakeWebServer.lastStatus == 202);
    assert(FakeWebServer.lastBody == "{\"restarting\":true}");
    assert(ESP32WiFiPortalTestAccess::restartPending(portal));
    assert(FakeESP.restartCalls == 0);
    FakeMillis = 100 + ESP32WiFiPortalTestAccess::restartDelay() - 1;
    portal.process();
    assert(FakeESP.restartCalls == 0);
    FakeMillis = 100 + ESP32WiFiPortalTestAccess::restartDelay();
    portal.process();
    assert(FakeESP.restartCalls == 1);
    assert(!ESP32WiFiPortalTestAccess::restartPending(portal));
    assert(FakePreferences.removeCalls == removeCallsBeforeReset);
    assert(FakePreferences.bytes == credentialsBeforeReset);
    // Explicit test cleanup; the reset path above did not erase this record.
    assert(portal.eraseCredentials(false));

    portal.process();
    portal.stopConfigPortal();
    expectFullyStopped(portal);

    assert(portal.setPortalIP(IPAddress(200, 5, 29, 8)));
    for (int cycle = 0; cycle < 500; ++cycle) {
      assert(portal.startConfigPortalAsync("Custom-Portal", "12345678", 0));
      assert(portal.isPortalActive());
      assert(FakeWiFi.runtimeIP == IPAddress(200, 5, 29, 8));
      assert(FakeDNS.address == IPAddress(200, 5, 29, 8));
      assert(std::string(
                 ESP32WiFiPortalTestAccess::redirectURL(portal).c_str()) ==
             "http://200.5.29.8/");
      portal.process();
      portal.stopConfigPortal();
      expectFullyStopped(portal);
    }

    // Rejected changes keep the previous known-good configuration intact.
    for (int attempt = 0; attempt < 500; ++attempt) {
      assert(!portal.setPortalIP(IPAddress(200, 5, 29, 0)));
      assert(portal.portalIP() == IPAddress(200, 5, 29, 8));
    }

    FakeWiFi.softAPConfigResult = false;
    assert(!portal.startConfigPortalAsync("Config-Failure", nullptr, 0));
    expectFullyStopped(portal);
    FakeWiFi.softAPConfigResult = true;

    FakeWiFi.softAPResult = false;
    assert(!portal.startConfigPortalAsync("AP-Failure", nullptr, 0));
    expectFullyStopped(portal);
    FakeWiFi.softAPResult = true;

    FakeWiFi.runtimeMismatch = true;
    assert(!portal.startConfigPortalAsync("Mismatch-Failure", nullptr, 0));
    expectFullyStopped(portal);
    FakeWiFi.runtimeMismatch = false;

    FakeDNS.startResult = false;
    assert(!portal.startConfigPortalAsync("DNS-Failure", nullptr, 0));
    expectFullyStopped(portal);
    FakeDNS.startResult = true;

    // Repeated reset requests are idempotent and unsigned timing survives wrap.
    assert(portal.startConfigPortalAsync("Reset \"Wrap\"\\Portal", nullptr, 0));
    FakeWebServer.handlers.at("/properties")();
    properties = FakeWebServer.lastBody;
    expectContains(properties,
                   R"JSON("ssid":"Reset \"Wrap\"\\Portal")JSON");
    FakeMillis = UINT32_MAX - 100U;
    FakeWebServer.handlers.at("/reset")();
    const uint32_t resetStartedAt =
        ESP32WiFiPortalTestAccess::restartRequestedAt(portal);
    FakeMillis += 20;
    FakeWebServer.handlers.at("/reset")();
    assert(ESP32WiFiPortalTestAccess::restartRequestedAt(portal) ==
           resetStartedAt);
    FakeMillis = resetStartedAt +
                 ESP32WiFiPortalTestAccess::restartDelay() - 1U;
    portal.process();
    assert(FakeESP.restartCalls == 1);
    FakeMillis = resetStartedAt + ESP32WiFiPortalTestAccess::restartDelay();
    portal.process();
    assert(FakeESP.restartCalls == 2);
    portal.process();
    assert(FakeESP.restartCalls == 2);
    portal.stopConfigPortal();
    expectFullyStopped(portal);

    // Unsigned millis subtraction must also stop cleanly across wrap-around.
    FakeMillis = UINT32_MAX - 5U;
    assert(portal.startConfigPortalAsync("Wraparound", nullptr, 10));
    FakeMillis = 8;
    portal.process();
    expectFullyStopped(portal);
    assert(portal.state() == ESP32WiFiPortal::State::Failed);
  }
  assert(FakeWebServer.liveInstances == 0);

  std::cout << "Portal lifecycle, cleanup, restart and wrap tests passed\n";
  return 0;
}
