#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include "host/TestAssert.h"
#include <iostream>
#include <string>

namespace {

void expectStopped(const ESP32WiFiPortal& portal) {
  assert(!portal.isPortalActive());
  assert(!FakeWiFi.apActive);
  assert(!FakeDNS.active);
  assert(FakeWebServer.liveInstances == 0);
}

void submitCandidate(ESP32WiFiPortal& portal) {
  FakeWebServer.args["ssid"] = "Candidate Router";
  FakeWebServer.args["password"] = "candidate-password";
  FakeWebServer.handlers.at("/save")();
  assert(FakeWebServer.lastStatus == 200);
  FakeMillis = 350;
  portal.process();
  FakeMillis = 370;
  portal.process();
  assert(FakeWiFi.beginCalls == 1);
}

}  // namespace

int main() {
  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    FakeWiFi.eventRegistrationResult = false;
    assert(!portal.startConfigPortalAsync("Event-Failure", "12345678"));
    assert(std::string(portal.lastError().c_str()) ==
           "Unable to register the Wi-Fi event handler");
    expectStopped(portal);

    FakeWiFi.eventRegistrationResult = true;
    assert(portal.startConfigPortalAsync("Event-Recovery", "12345678"));
    portal.stopConfigPortal();
    expectStopped(portal);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    FakeWiFi.autoReconnectResult = false;
    assert(!portal.startConfigPortalAsync("Policy-Failure", "12345678"));
    assert(std::string(portal.lastError().c_str()) ==
           "Unable to disable the Arduino Wi-Fi reconnect policy");
    expectStopped(portal);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    FakeWiFi.modeResult = false;
    assert(!portal.startConfigPortalAsync("Mode-Failure", "12345678"));
    assert(std::string(portal.lastError().c_str()) ==
           "Failed to enable ESP32 AP+STA mode");
    expectStopped(portal);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    FakeWiFi.storageResult = false;
    assert(!portal.startConfigPortalAsync("Storage-Failure", "12345678"));
    assert(std::string(portal.lastError().c_str()) ==
           "Failed to select RAM-only Wi-Fi driver storage");
    expectStopped(portal);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.setHostname("12345678901234567890123456789012"));
    assert(std::string(portal.lastError().c_str()) ==
           "Hostname must contain at most 31 bytes");
    assert(portal.setHostname("ewp-device"));
    assert(portal.startConfigPortalAsync("Ordering", "12345678"));
    assert(FakeWiFi.persistentCalls == 1);
    assert(FakeWiFi.persistentCallOrder < FakeWiFi.hostnameCallOrder);
    assert(FakeWiFi.hostnameCallOrder < FakeWiFi.modeCallOrder);
    assert(FakeWiFi.modeCallOrder < FakeWiFi.storageCallOrder);
    assert(!portal.setHostname("too-late"));
    assert(std::string(portal.lastError().c_str()) ==
           "Hostname must be configured before the first Wi-Fi operation");
    portal.stopConfigPortal();
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.startConfigPortalAsync("Stop-Fallback", "12345678"));
    FakeWiFi.softAPDisconnectResult = false;
    portal.stopConfigPortal();
    expectStopped(portal);
    assert(FakeWiFi.mode == WIFI_STA);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal second;
    second.setLogging(false);
    {
      ESP32WiFiPortal first;
      first.setLogging(false);
      assert(first.startConfigPortalAsync("First", "12345678"));
      assert(!second.startConfigPortalAsync("Second", "12345678"));
      assert(std::string(second.lastError().c_str()) ==
             "Wi-Fi is already managed by another ESP32WiFiPortal instance");
      first.stopConfigPortal();
      // Stopping the Portal does not release the process-wide event/reconnect
      // owner while the manager remains alive.
      assert(!second.startConfigPortalAsync("Second", "12345678"));
    }
    assert(second.startConfigPortalAsync("Second", "12345678"));
    second.stopConfigPortal();
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal second;
    second.setLogging(false);
    {
      ESP32WiFiPortal first;
      first.setLogging(false);
      assert(first.startConfigPortalAsync("Owner", "12345678"));

      const uint32_t policyCalls = FakeWiFi.autoReconnectCalls;
      const uint32_t persistentCalls = FakeWiFi.persistentCalls;
      const uint32_t modeCalls = FakeWiFi.modeCalls;
      const bool policy = FakeWiFi.autoReconnect;
      second.setAutoReconnect(false);

      assert(second.autoReconnectEnabled());
      assert(std::string(second.lastError().c_str()) ==
             "Wi-Fi is already managed by another ESP32WiFiPortal instance");
      assert(FakeWiFi.autoReconnectCalls == policyCalls);
      assert(FakeWiFi.persistentCalls == persistentCalls);
      assert(FakeWiFi.modeCalls == modeCalls);
      assert(FakeWiFi.autoReconnect == policy);
      first.stopConfigPortal();
    }

    // The destructor releases ownership and the waiting manager can then
    // acquire it normally.
    second.setAutoReconnect(false);
    assert(!second.autoReconnectEnabled());
    assert(std::string(second.lastError().c_str()).empty());
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal second;
    second.setLogging(false);
    {
      ESP32WiFiPortal first;
      first.setLogging(false);
      assert(ESP32WiFiPortalTestAccess::saveCredentials(
          first, String("Stored Router"), String("stored-password")));
      assert(first.startConfigPortalAsync("Owner-Erase", "12345678"));
      FakeWiFi.status = WL_CONNECTED;

      const uint32_t disconnectCalls = FakeWiFi.disconnectCalls;
      const uint32_t policyCalls = FakeWiFi.autoReconnectCalls;
      const uint32_t persistentCalls = FakeWiFi.persistentCalls;
      const uint32_t modeCalls = FakeWiFi.modeCalls;
      const uint32_t softAPDisconnectCalls = FakeWiFi.softAPDisconnectCalls;
      assert(!second.eraseCredentials(true));

      assert(FakePreferences.bytes.empty());
      assert(FakeWiFi.disconnectCalls == disconnectCalls);
      assert(FakeWiFi.autoReconnectCalls == policyCalls);
      assert(FakeWiFi.persistentCalls == persistentCalls);
      assert(FakeWiFi.modeCalls == modeCalls);
      assert(FakeWiFi.softAPDisconnectCalls == softAPDisconnectCalls);
      assert(FakeWiFi.status == WL_CONNECTED);
      assert(std::string(second.lastError().c_str()) ==
             "Credentials erased, but Wi-Fi disconnect requires ownership");
      first.stopConfigPortal();
    }
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Reconnect Router"), String("reconnect-password")));
    portal.setAutoReconnect(true);
    FakeMillis = 1000;
    portal.process();
    FakeMillis = 1020;
    portal.process();
    assert(ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));

    FakeWiFi.status = WL_CONNECTED;
    FakeWiFi.disconnectResult = false;
    portal.setAutoReconnect(false);
    assert(!portal.autoReconnectEnabled());
    assert(portal.state() == ESP32WiFiPortal::State::Connected);
    assert(std::string(portal.lastError().c_str()) ==
           "Failed to cancel the STA connection attempt");
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.startConfigPortalAsync(
        "123456789012345678901234567890123", "12345678"));
    assert(FakeWiFi.modeCalls == 0);
    assert(std::string(portal.lastError().c_str()) ==
           "AP SSID must contain 1-32 bytes");
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.startConfigPortalAsync("Event-Coalescing", "12345678"));
    submitCandidate(portal);

    emitWiFiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
                  WIFI_REASON_AUTH_FAIL);
    emitWiFiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
                  WIFI_REASON_UNSPECIFIED);
    portal.process();

    assert(portal.state() == ESP32WiFiPortal::State::Portal);
    assert(portal.lastDisconnectReason() == WIFI_REASON_UNSPECIFIED);
    assert(std::string(portal.lastError().c_str()).find("authentication failed") !=
           std::string::npos);
    portal.stopConfigPortal();
  }

  resetFakeRuntime();
  FakeWiFi.status = WL_CONNECTED;
  FakeWiFi.disconnectResult = false;
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Saved Router"), String("saved-password")));
    assert(!portal.connectSaved(1000));
    assert(std::string(portal.lastError().c_str()) ==
           "Failed to disconnect the current STA connection");
  }

  resetFakeRuntime();
  assert(FakeWiFi.autoReconnect);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.startConfigPortalAsync("Restore-Policy", "12345678"));
    assert(!FakeWiFi.autoReconnect);
    portal.stopConfigPortal();
  }
  assert(FakeWiFi.autoReconnect);

  std::cout << "Wi-Fi driver ownership, persistence, ordering and event tests passed\n";
  return 0;
}
