#include "host/FakeRuntime.h"

#include <ESP32WiFiPortal.h>
#include "host/TestAssert.h"
#include <cstdint>
#include <iostream>
#include <string>

namespace {

void startPortal(ESP32WiFiPortal& portal) {
  portal.setLogging(false);
  assert(portal.startConfigPortalAsync("Scan-Test", "12345678"));
}

void requestScan() { FakeWebServer.handlers.at("/scan")(); }

}  // namespace

int main() {
  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    startPortal(portal);
    FakeWiFi.scanStartResult = WIFI_SCAN_RUNNING;
    FakeWiFi.scanCompleteResult = WIFI_SCAN_RUNNING;

    const uint32_t before = FakeMillis;
    requestScan();
    assert(FakeMillis == before);
    assert(FakeWebServer.lastStatus == 202);
    assert(FakeWebServer.responseHeaders["Retry-After"] == "1");
    assert(FakeWiFi.scanNetworksCalls == 1);

    FakeWiFi.scanSSIDs = {String("Strong \"Router\""), String("Duplicate"),
                          String("Duplicate"), String("")};
    FakeWiFi.scanRSSI = {-35, -50, -80, -90};
    FakeWiFi.scanEncryption = {WIFI_AUTH_WPA2_PSK, WIFI_AUTH_OPEN,
                               WIFI_AUTH_WPA2_PSK, WIFI_AUTH_OPEN};
    FakeWiFi.scanCompleteResult = 4;
    portal.process();
    requestScan();
    assert(FakeWebServer.lastStatus == 200);
    assert(FakeWebServer.lastContentType ==
           "application/json; charset=utf-8");
    const std::string body = FakeWebServer.lastBody;
    assert(body.find("Strong \\\"Router\\\"") != std::string::npos);
    assert(body.find("\"ssid\":\"Duplicate\"") != std::string::npos);
    assert(body.find("\"rssi\":-35") != std::string::npos);
    assert(body.find("\"open\":true") != std::string::npos);
    assert(body.find("\"open\":false") != std::string::npos);
    const size_t firstDuplicate = body.find("\"ssid\":\"Duplicate\"");
    assert(body.find("\"ssid\":\"Duplicate\"", firstDuplicate + 1) ==
           std::string::npos);
    assert(FakeWiFi.scanDeleteCalls == 1);
    portal.stopConfigPortal();
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    startPortal(portal);
    FakeWiFi.scanStartResult = WIFI_SCAN_FAILED;
    requestScan();
    assert(FakeWebServer.lastStatus == 503);
    assert(FakeWiFi.scanDeleteCalls >= 1);
    portal.stopConfigPortal();
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    startPortal(portal);
    FakeMillis = UINT32_MAX - 5000U;
    FakeWiFi.scanStartResult = WIFI_SCAN_RUNNING;
    FakeWiFi.scanCompleteResult = WIFI_SCAN_RUNNING;
    requestScan();
    assert(FakeWebServer.lastStatus == 202);

    FakeMillis += 14999U;
    portal.process();
    assert(FakeWiFi.scanStopCalls == 0);
    FakeMillis += 1U;
    portal.process();
    assert(FakeWiFi.scanStopCalls == 1);
    requestScan();
    assert(FakeWebServer.lastStatus == 503);
    portal.stopConfigPortal();
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    startPortal(portal);
    FakeWiFi.scanStartResult = WIFI_SCAN_RUNNING;
    FakeWiFi.scanCompleteResult = WIFI_SCAN_RUNNING;
    requestScan();
    portal.stopConfigPortal();
    assert(FakeWiFi.scanStopCalls == 1);
    assert(FakeWiFi.scanDeleteCalls == 1);
  }

  std::cout << "Asynchronous scan, deduplication, timeout and cleanup tests passed\n";
  return 0;
}
