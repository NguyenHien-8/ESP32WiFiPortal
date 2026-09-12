#pragma once

#include <Arduino.h>

#include <cstdint>
#include <functional>
#include <vector>

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

enum wifi_mode_t : uint8_t {
  WIFI_MODE_NULL = 0,
  WIFI_STA = 1,
  WIFI_AP_STA = 3
};
enum wifi_auth_mode_t : uint8_t { WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK };

constexpr int16_t WIFI_SCAN_RUNNING = -1;
constexpr int16_t WIFI_SCAN_FAILED = -2;
constexpr uint8_t WIFI_REASON_UNSPECIFIED = 1;
constexpr uint8_t WIFI_REASON_AUTH_FAIL = 2;
constexpr uint8_t WIFI_REASON_802_1X_AUTH_FAILED = 3;
constexpr uint8_t WIFI_REASON_ASSOC_LEAVE = 4;

struct FakeWiFiState {
  bool autoReconnect = true;
  bool autoReconnectResult = true;
  bool eventRegistrationResult = true;
  bool modeResult = true;
  bool hostnameResult = true;
  bool softAPHostnameResult = true;
  bool storageResult = true;
  bool disconnectResult = true;
  bool softAPDisconnectResult = true;
  bool scanStopResult = true;
  bool softAPConfigResult = true;
  bool softAPResult = true;
  bool runtimeMismatch = false;
  bool configResult = true;
  bool apActive = false;
  uint32_t softAPConfigCalls = 0;
  uint32_t softAPCalls = 0;
  uint32_t softAPDisconnectCalls = 0;
  uint32_t disconnectCalls = 0;
  uint32_t beginCalls = 0;
  uint32_t scanNetworksCalls = 0;
  uint32_t scanDeleteCalls = 0;
  uint32_t scanStopCalls = 0;
  uint32_t persistentCalls = 0;
  uint32_t autoReconnectCalls = 0;
  uint32_t modeCalls = 0;
  uint32_t storageCalls = 0;
  uint32_t callSequence = 0;
  uint32_t persistentCallOrder = 0;
  uint32_t hostnameCallOrder = 0;
  uint32_t modeCallOrder = 0;
  uint32_t storageCallOrder = 0;
  wifi_mode_t mode = WIFI_MODE_NULL;
  IPAddress requestedIP;
  IPAddress requestedGateway;
  IPAddress requestedSubnet;
  IPAddress runtimeIP;
  IPAddress runtimeSubnet;
  IPAddress staIP;
  IPAddress staGateway;
  IPAddress staSubnet;
  uint8_t apMAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
  uint8_t staMAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
  wl_status_t status = WL_DISCONNECTED;
  wl_status_t beginResult = WL_IDLE_STATUS;
  int16_t scanStartResult = WIFI_SCAN_FAILED;
  int16_t scanCompleteResult = WIFI_SCAN_FAILED;
  String lastSSID;
  String lastPassword;
  std::vector<String> scanSSIDs;
  std::vector<int32_t> scanRSSI;
  std::vector<wifi_auth_mode_t> scanEncryption;
  std::function<void(arduino_event_id_t, arduino_event_info_t)> eventHandler;
};

extern FakeWiFiState FakeWiFi;

class WiFiClass {
public:
  bool getAutoReconnect() const { return FakeWiFi.autoReconnect; }
  bool setAutoReconnect(bool enabled) {
    ++FakeWiFi.autoReconnectCalls;
    if (!FakeWiFi.autoReconnectResult) return false;
    FakeWiFi.autoReconnect = enabled;
    return true;
  }

  void persistent(bool) {
    ++FakeWiFi.persistentCalls;
    FakeWiFi.persistentCallOrder = ++FakeWiFi.callSequence;
  }

  template <typename Handler>
  wifi_event_id_t onEvent(Handler handler) {
    if (!FakeWiFi.eventRegistrationResult) return 0;
    FakeWiFi.eventHandler = handler;
    return 1;
  }

  void removeEvent(wifi_event_id_t) { FakeWiFi.eventHandler = nullptr; }
  wl_status_t status() const { return FakeWiFi.status; }
  bool mode(wifi_mode_t mode) {
    ++FakeWiFi.modeCalls;
    FakeWiFi.modeCallOrder = ++FakeWiFi.callSequence;
    if (!FakeWiFi.modeResult) return false;
    FakeWiFi.mode = mode;
    if (mode == WIFI_STA || mode == WIFI_MODE_NULL) {
      FakeWiFi.apActive = false;
    }
    return true;
  }
  bool setHostname(const char*) {
    FakeWiFi.hostnameCallOrder = ++FakeWiFi.callSequence;
    return FakeWiFi.hostnameResult;
  }
  bool softAPsetHostname(const char*) {
    return FakeWiFi.softAPHostnameResult;
  }

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
  uint8_t* softAPmacAddress(uint8_t* address) const {
    if (address) memcpy(address, FakeWiFi.apMAC, sizeof(FakeWiFi.apMAC));
    return address;
  }
  bool softAPdisconnect(bool) {
    ++FakeWiFi.softAPDisconnectCalls;
    if (!FakeWiFi.softAPDisconnectResult) return false;
    FakeWiFi.apActive = false;
    return true;
  }

  int16_t scanComplete() const { return FakeWiFi.scanCompleteResult; }
  void scanDelete() { ++FakeWiFi.scanDeleteCalls; }
  int16_t scanNetworks(bool, bool) {
    ++FakeWiFi.scanNetworksCalls;
    return FakeWiFi.scanStartResult;
  }
  String SSID(int index) const {
    return index >= 0 && static_cast<size_t>(index) < FakeWiFi.scanSSIDs.size()
               ? FakeWiFi.scanSSIDs[static_cast<size_t>(index)]
               : String();
  }
  int32_t RSSI(int index) const {
    return index >= 0 && static_cast<size_t>(index) < FakeWiFi.scanRSSI.size()
               ? FakeWiFi.scanRSSI[static_cast<size_t>(index)]
               : -100;
  }
  wifi_auth_mode_t encryptionType(int index) const {
    return index >= 0 &&
                   static_cast<size_t>(index) < FakeWiFi.scanEncryption.size()
               ? FakeWiFi.scanEncryption[static_cast<size_t>(index)]
               : WIFI_AUTH_OPEN;
  }

  bool disconnect(bool = false, bool = false) {
    ++FakeWiFi.disconnectCalls;
    if (!FakeWiFi.disconnectResult) return false;
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

  wl_status_t begin(const char* ssid, const char* password) {
    ++FakeWiFi.beginCalls;
    FakeWiFi.lastSSID = ssid;
    FakeWiFi.lastPassword = password;
    return FakeWiFi.beginResult;
  }
  IPAddress localIP() const { return FakeWiFi.staIP; }
  IPAddress gatewayIP() const { return FakeWiFi.staGateway; }
  IPAddress subnetMask() const { return FakeWiFi.staSubnet; }
  uint8_t* macAddress(uint8_t* address) const {
    if (address) memcpy(address, FakeWiFi.staMAC, sizeof(FakeWiFi.staMAC));
    return address;
  }
  const char* disconnectReasonName(wifi_err_reason_t) const { return "fake"; }
};

extern WiFiClass WiFi;
