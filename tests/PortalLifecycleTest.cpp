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
    assert(hasRoute("/generate_204"));
    assert(hasRoute("/gen_204"));
    assert(hasRoute("/hotspot-detect.html"));
    assert(hasRoute("/library/test/success.html"));
    assert(hasRoute("/connecttest.txt"));
    assert(hasRoute("/ncsi.txt"));
    assert(hasRoute("/fwlink"));
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
