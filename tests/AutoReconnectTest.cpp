#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

void seedCredentials(ESP32WiFiPortal& portal) {
  assert(ESP32WiFiPortalTestAccess::saveCredentials(
      portal, String("Test Router"), String("test-password")));
}

uint32_t beginFirstReconnect(ESP32WiFiPortal& portal,
                             uint32_t scheduledAt = 0) {
  FakeMillis = scheduledAt;
  portal.setAutoReconnect(false);
  portal.setAutoReconnect(true);
  const uint32_t before = FakeMillis;
  portal.process();
  assert(FakeMillis == before);
  FakeMillis = scheduledAt + 1000;
  portal.process();
  assert(FakeWiFi.beginCalls == 0);
  FakeMillis += 20;
  portal.process();
  assert(FakeWiFi.beginCalls == 1);
  assert(ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
  return ESP32WiFiPortalTestAccess::connectAttemptAt(portal);
}

void expectFiniteTimeout(uint32_t timeoutMs) {
  resetFakeRuntime();
  ESP32WiFiPortal portal;
  portal.setLogging(false);
  seedCredentials(portal);
  assert(portal.setConnectionRetryPolicy(0, 1000, 60000));
  portal.setConnectTimeout(timeoutMs);
  const uint32_t attemptAt = beginFirstReconnect(portal);

  FakeMillis = attemptAt + timeoutMs - 1;
  const uint32_t before = FakeMillis;
  portal.process();
  assert(FakeMillis == before);
  assert(ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));

  FakeMillis = attemptAt + timeoutMs;
  portal.process();
  assert(!ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
  assert(ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
  assert(ESP32WiFiPortalTestAccess::reconnectDelay(portal) == 60000);
}

}  // namespace

int main() {
  expectFiniteTimeout(10000);
  expectFiniteTimeout(15000);

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    portal.setConnectTimeout(0);
    assert(ESP32WiFiPortalTestAccess::connectTimeout(portal) == 15000);
    seedCredentials(portal);
    assert(portal.setConnectionRetryPolicy(0, 1000, 60000));
    const uint32_t attemptAt = beginFirstReconnect(portal);
    FakeMillis = attemptAt + 14999;
    portal.process();
    assert(ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
    FakeMillis = attemptAt + 15000;
    portal.process();
    assert(!ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
    assert(ESP32WiFiPortalTestAccess::reconnectDelay(portal) == 60000);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    seedCredentials(portal);
    portal.setAutoReconnect(false);
    const uint32_t startedAt = FakeMillis;
    assert(!portal.connectSaved(0));
    const uint32_t elapsed = FakeMillis - startedAt;
    assert(elapsed >= 15000);
    assert(elapsed < 15100);
    assert(FakeWiFi.beginCalls == 1);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    seedCredentials(portal);
    assert(portal.setConnectionRetryPolicy(1, 1000, 60000));
    portal.setConnectTimeout(10000);
    uint32_t attemptAt = beginFirstReconnect(portal);

    FakeMillis = attemptAt + 10000;
    portal.process();
    assert(ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
    assert(ESP32WiFiPortalTestAccess::reconnectRetriesUsed(portal) == 1);
    assert(ESP32WiFiPortalTestAccess::reconnectDelay(portal) == 1000);

    FakeMillis += 1000;
    portal.process();
    portal.process();
    assert(FakeWiFi.beginCalls == 2);
    attemptAt = ESP32WiFiPortalTestAccess::connectAttemptAt(portal);
    FakeMillis = attemptAt + 10000;
    portal.process();
    assert(ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
    assert(ESP32WiFiPortalTestAccess::reconnectRetriesUsed(portal) == 0);
    assert(ESP32WiFiPortalTestAccess::reconnectDelay(portal) == 60000);

    int connectedCallbacks = 0;
    portal.onConnected([&connectedCallbacks]() { ++connectedCallbacks; });
    FakeMillis += 60000;
    portal.process();
    portal.process();
    assert(FakeWiFi.beginCalls == 3);
    FakeWiFi.status = WL_CONNECTED;
    portal.process();
    assert(portal.state() == ESP32WiFiPortal::State::Connected);
    assert(!ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
    assert(connectedCallbacks == 1);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    seedCredentials(portal);
    assert(portal.setConnectionRetryPolicy(0, 1000, 60000));
    portal.setConnectTimeout(10000);

    const uint32_t scheduledAt = UINT32_MAX - 500U;
    FakeMillis = scheduledAt;
    portal.setAutoReconnect(false);
    portal.setAutoReconnect(true);
    FakeMillis = 498;
    portal.process();
    assert(FakeWiFi.beginCalls == 0);
    FakeMillis = 499;
    portal.process();
    FakeMillis = 519;
    portal.process();
    assert(FakeWiFi.beginCalls == 1);

    const uint32_t attemptAt =
        ESP32WiFiPortalTestAccess::connectAttemptAt(portal);
    FakeMillis = attemptAt + 9999U;
    portal.process();
    assert(ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
    FakeMillis = attemptAt + 10000U;
    portal.process();
    assert(!ESP32WiFiPortalTestAccess::reconnectAttemptActive(portal));
    assert(ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
  }

  std::cout << "Finite reconnect timeout, retry, cooldown and wrap tests passed\n";
  return 0;
}
