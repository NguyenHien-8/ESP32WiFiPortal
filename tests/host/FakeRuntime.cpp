#include "FakeRuntime.h"

#include <cassert>

FakeSerialClass Serial;
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
  FakeDNS = FakeDNSServerState();
  FakeWebServer = FakeWebServerState();
  FakeWiFi = FakeWiFiState();
  FakePreferences = FakePreferencesState();
  if (preservePreferences) {
    FakePreferences.bytes = savedBytes;
    FakePreferences.strings = savedStrings;
  }
}
