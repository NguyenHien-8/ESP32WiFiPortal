#pragma once

#include <WiFi.h>

constexpr int ESP_OK = 0;

enum wifi_storage_t : uint8_t { WIFI_STORAGE_FLASH, WIFI_STORAGE_RAM };

inline int esp_wifi_scan_stop() {
  ++FakeWiFi.scanStopCalls;
  return FakeWiFi.scanStopResult ? ESP_OK : -1;
}

inline int esp_wifi_set_storage(wifi_storage_t) {
  ++FakeWiFi.storageCalls;
  FakeWiFi.storageCallOrder = ++FakeWiFi.callSequence;
  return FakeWiFi.storageResult ? ESP_OK : -1;
}
