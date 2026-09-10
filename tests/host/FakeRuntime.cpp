#include "FakeRuntime.h"

#include <cassert>

FakeSerialClass Serial;
uint32_t FakeMillis = 0;
FakeDNSServerState FakeDNS;
FakeWebServerState FakeWebServer;
FakeWiFiState FakeWiFi;
WiFiClass WiFi;

uint32_t millis() { return FakeMillis; }

void delay(uint32_t milliseconds) { FakeMillis += milliseconds; }

void yield() {}

void resetFakeRuntime() {
  assert(FakeWebServer.liveInstances == 0);
  FakeMillis = 0;
  FakeDNS = FakeDNSServerState();
  FakeWebServer = FakeWebServerState();
  FakeWiFi = FakeWiFiState();
}
