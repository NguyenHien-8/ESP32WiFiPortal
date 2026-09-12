#include "FakeRuntime.h"

#include "TestAssert.h"

FakeSerialClass Serial;
FakeESPState FakeESP;
ESPClass ESP;
uint32_t FakeMillis = 0;
FakeDNSServerState FakeDNS;
FakeWebServerState FakeWebServer;
FakeWiFiState FakeWiFi;
FakePreferencesState FakePreferences;
WiFiClass WiFi;

uint32_t millis() { return FakeMillis; }

void delay(uint32_t milliseconds) { FakeMillis += milliseconds; }

void yield() {}

void resetFakeRuntime(bool preservePreferences) {
  assert(FakeWebServer.liveInstances == 0);
  const auto savedBytes = FakePreferences.bytes;
  const auto savedStrings = FakePreferences.strings;
  FakeMillis = 0;
  FakeESP = FakeESPState();
  FakeDNS = FakeDNSServerState();
  FakeWebServer = FakeWebServerState();
  FakeWiFi = FakeWiFiState();
  FakePreferences = FakePreferencesState();
  if (preservePreferences) {
    FakePreferences.bytes = savedBytes;
    FakePreferences.strings = savedStrings;
  }
}

void emitWiFiEvent(arduino_event_id_t event, uint8_t reason) {
  if (!FakeWiFi.eventHandler) return;
  arduino_event_info_t info;
  info.wifi_sta_disconnected.reason = reason;
  FakeWiFi.eventHandler(event, info);
}
