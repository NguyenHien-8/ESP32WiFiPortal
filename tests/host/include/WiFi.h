#pragma once

#include <Arduino.h>

#include <cstdint>

using wifi_event_id_t = uint32_t;
using wifi_err_reason_t = uint8_t;

enum arduino_event_id_t : uint8_t {
  ARDUINO_EVENT_WIFI_STA_CONNECTED,
  ARDUINO_EVENT_WIFI_STA_GOT_IP,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED
};

struct arduino_event_info_t {
  struct {
    uint8_t reason = 0;
  } wifi_sta_disconnected;
};

enum wl_status_t : uint8_t {
  WL_IDLE_STATUS,
  WL_CONNECTED,
  WL_CONNECT_FAILED,
  WL_DISCONNECTED
};

enum wifi_mode_t : uint8_t { WIFI_STA, WIFI_AP_STA };
enum wifi_auth_mode_t : uint8_t { WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK };

constexpr int16_t WIFI_SCAN_RUNNING = -1;
constexpr int16_t WIFI_SCAN_FAILED = -2;
constexpr uint8_t WIFI_REASON_UNSPECIFIED = 1;
constexpr uint8_t WIFI_REASON_AUTH_FAIL = 2;
constexpr uint8_t WIFI_REASON_802_1X_AUTH_FAILED = 3;
constexpr uint8_t WIFI_REASON_ASSOC_LEAVE = 4;

struct FakeWiFiState {
  bool autoReconnect = true;
  bool softAPConfigResult = true;
  bool softAPResult = true;
  bool runtimeMismatch = false;
  bool configResult = true;
  bool apActive = false;
  uint32_t softAPConfigCalls = 0;
  uint32_t softAPCalls = 0;
  uint32_t softAPDisconnectCalls = 0;
  uint32_t disconnectCalls = 0;
  IPAddress requestedIP;
  IPAddress requestedGateway;
  IPAddress requestedSubnet;
  IPAddress runtimeIP;
  IPAddress runtimeSubnet;
  IPAddress staIP;
  wl_status_t status = WL_DISCONNECTED;
  wl_status_t beginResult = WL_IDLE_STATUS;
};

extern FakeWiFiState FakeWiFi;

class WiFiClass {
public:
  bool getAutoReconnect() const { return FakeWiFi.autoReconnect; }
  bool setAutoReconnect(bool enabled) {
    FakeWiFi.autoReconnect = enabled;
    return true;
  }

  template <typename Handler>
  wifi_event_id_t onEvent(Handler) {
    return 1;
  }

  void removeEvent(wifi_event_id_t) {}
  wl_status_t status() const { return FakeWiFi.status; }
  bool mode(wifi_mode_t) { return true; }
  bool setHostname(const char*) { return true; }
  bool softAPsetHostname(const char*) { return true; }

  bool softAPConfig(const IPAddress& local,
                    const IPAddress& gateway,
                    const IPAddress& subnet) {
    ++FakeWiFi.softAPConfigCalls;
    FakeWiFi.requestedIP = local;
    FakeWiFi.requestedGateway = gateway;
    FakeWiFi.requestedSubnet = subnet;
    if (!FakeWiFi.softAPConfigResult) return false;
    FakeWiFi.runtimeIP = FakeWiFi.runtimeMismatch ? IPAddress(192, 0, 2, 1)
                                                 : local;
    FakeWiFi.runtimeSubnet = subnet;
    return true;
  }

  bool softAP(const char*, const char*, uint8_t, int) {
    ++FakeWiFi.softAPCalls;
    FakeWiFi.apActive = FakeWiFi.softAPResult;
    return FakeWiFi.softAPResult;
  }

  IPAddress softAPIP() const { return FakeWiFi.runtimeIP; }
  IPAddress softAPSubnetMask() const { return FakeWiFi.runtimeSubnet; }
  bool softAPdisconnect(bool) {
    ++FakeWiFi.softAPDisconnectCalls;
    FakeWiFi.apActive = false;
    return true;
  }

  int16_t scanComplete() const { return WIFI_SCAN_FAILED; }
  void scanDelete() {}
  int16_t scanNetworks(bool, bool) { return WIFI_SCAN_FAILED; }
  String SSID(int) const { return String(); }
  int32_t RSSI(int) const { return -100; }
  wifi_auth_mode_t encryptionType(int) const { return WIFI_AUTH_OPEN; }

  bool disconnect(bool = false, bool = false) {
    ++FakeWiFi.disconnectCalls;
    FakeWiFi.status = WL_DISCONNECTED;
    return true;
  }

  bool config(const IPAddress& local,
              const IPAddress&,
              const IPAddress&,
              const IPAddress&,
              const IPAddress&) {
    FakeWiFi.staIP = local;
    return FakeWiFi.configResult;
  }

  wl_status_t begin(const char*, const char*) { return FakeWiFi.beginResult; }
  IPAddress localIP() const { return FakeWiFi.staIP; }
  const char* disconnectReasonName(wifi_err_reason_t) const { return "fake"; }
};

extern WiFiClass WiFi;
