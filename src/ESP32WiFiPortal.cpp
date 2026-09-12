/**
 * @file ESP32WiFiPortal.cpp
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief ESP32 Wi-Fi captive portal library implementation
 * @version 2.1.2
 * @date 2026-09-12
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include "ESP32WiFiPortal.h"
#include "PortalPage.h"

#include <esp_wifi.h>
#include <new>

namespace {
void appendJsonEscaped(String& output, const String& value) {
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t character = static_cast<uint8_t>(value[i]);
    switch (character) {
      case '"': output += F("\\\""); break;
      case '\\': output += F("\\\\"); break;
      case '\b': output += F("\\b"); break;
      case '\f': output += F("\\f"); break;
      case '\n': output += F("\\n"); break;
      case '\r': output += F("\\r"); break;
      case '\t': output += F("\\t"); break;
      default:
        if (character < 0x20) {
          output += F("\\u00");
          output += kHex[character >> 4];
          output += kHex[character & 0x0F];
        } else {
          output += static_cast<char>(character);
        }
        break;
    }
  }
}

void appendIPAddress(String& output, const IPAddress& address) {
  for (uint8_t i = 0; i < 4; ++i) {
    if (i > 0) output += '.';
    output += static_cast<unsigned int>(address[i]);
  }
}

void appendHex64(String& output, uint64_t value) {
  static const char kHex[] = "0123456789ABCDEF";
  for (int8_t shift = 60; shift >= 0; shift -= 4) {
    output += kHex[(value >> shift) & 0x0F];
  }
}

void appendMACAddress(String& output, const uint8_t* address) {
  static const char kHex[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) output += ':';
    output += kHex[address[i] >> 4];
    output += kHex[address[i] & 0x0F];
  }
}

uint32_t hashSSID(const String& ssid) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < ssid.length(); ++i) {
    hash ^= static_cast<uint8_t>(ssid[i]);
    hash *= 16777619UL;
  }
  return hash;
}

// Arduino-ESP32 exposes one process-wide WiFi object. Two portal instances
// installing callbacks and reconnect policies against it cannot be isolated,
// so ownership is explicit and deterministic instead of allowing races.
ESP32WiFiPortal* activeWiFiOwner = nullptr;
}  // namespace

constexpr uint16_t ESP32WiFiPortal::kDnsPort;
constexpr uint16_t ESP32WiFiPortal::kHttpPort;
constexpr const char* ESP32WiFiPortal::kPrefsNamespace;
constexpr const char* ESP32WiFiPortal::kPrefsCredential;
constexpr const char* ESP32WiFiPortal::kPrefsCredentialBackup;
constexpr const char* ESP32WiFiPortal::kPrefsCredentialEraseMarker;
constexpr const char* ESP32WiFiPortal::kPrefsSSID;
constexpr const char* ESP32WiFiPortal::kPrefsPassword;
constexpr uint32_t ESP32WiFiPortal::kCredentialMagic;
constexpr uint16_t ESP32WiFiPortal::kCredentialVersion;
constexpr size_t ESP32WiFiPortal::kCredentialSSIDCapacity;
constexpr size_t ESP32WiFiPortal::kCredentialPasswordCapacity;
constexpr size_t ESP32WiFiPortal::kCredentialSSIDOffset;
constexpr size_t ESP32WiFiPortal::kCredentialPasswordOffset;
constexpr size_t ESP32WiFiPortal::kCredentialCRCOffset;
constexpr size_t ESP32WiFiPortal::kCredentialRecordSize;
constexpr uint32_t ESP32WiFiPortal::kDefaultConnectTimeoutMs;
constexpr uint32_t ESP32WiFiPortal::kRestartDelayMs;

const char* ESP32WiFiPortal::portalNetworkValidationMessage(
    PortalNetworkValidationResult result) {
  switch (result) {
    case PortalNetworkValidationResult::InvalidLocalIP:
      return "Portal IP is not a usable unicast IPv4 address";
    case PortalNetworkValidationResult::InvalidGateway:
      return "Portal gateway is not a usable unicast IPv4 address";
    case PortalNetworkValidationResult::InvalidSubnetMask:
      return "Portal subnet mask is invalid or non-contiguous";
    case PortalNetworkValidationResult::UnsupportedSubnet:
      return "Portal subnet must be between /24 and /28 for SoftAP DHCP";
    case PortalNetworkValidationResult::DifferentSubnet:
      return "Portal IP and gateway must be in the same subnet";
    case PortalNetworkValidationResult::LocalIsNetworkAddress:
      return "Portal IP cannot be the subnet network address";
    case PortalNetworkValidationResult::LocalIsBroadcastAddress:
      return "Portal IP cannot be the subnet broadcast address";
    case PortalNetworkValidationResult::GatewayIsNetworkAddress:
      return "Portal gateway cannot be the subnet network address";
    case PortalNetworkValidationResult::GatewayIsBroadcastAddress:
      return "Portal gateway cannot be the subnet broadcast address";
    case PortalNetworkValidationResult::LocalConflictsWithDHCPLease:
      return "Portal IP conflicts with the SoftAP DHCP lease range";
    case PortalNetworkValidationResult::GatewayConflictsWithDHCPLease:
      return "Portal gateway conflicts with the SoftAP DHCP lease range";
    case PortalNetworkValidationResult::Valid:
      return "";
  }
  return "Portal IPv4 configuration is invalid";
}

ESP32WiFiPortal::ESP32WiFiPortal()
    : _portalIP(192, 168, 4, 1),
      _portalGateway(192, 168, 4, 1),
      _portalSubnet(255, 255, 255, 0) {}

ESP32WiFiPortal::~ESP32WiFiPortal() {
  const bool restoreCoreAutoReconnect = activeWiFiOwner == this;
  if (_wifiEventHandlerId != 0) {
    WiFi.removeEvent(_wifiEventHandlerId);
    _wifiEventHandlerId = 0;
  }
  stopConfigPortal();
  cancelAutoReconnect(true);
  if (restoreCoreAutoReconnect) {
    if (!WiFi.setAutoReconnect(_coreAutoReconnectWasEnabled) &&
        _loggingEnabled) {
      Serial.println(F("[EWP] Failed to restore core auto reconnect policy"));
    }
    activeWiFiOwner = nullptr;
  }
}

bool ESP32WiFiPortal::ensureWiFiEventHandler() {
  if (_wifiEventHandlerId != 0) return true;
  if (activeWiFiOwner && activeWiFiOwner != this) {
    setError("Wi-Fi is already managed by another ESP32WiFiPortal instance");
    return false;
  }

  activeWiFiOwner = this;

  _coreAutoReconnectWasEnabled = WiFi.getAutoReconnect();
  // This library owns credential persistence. Keeping the Arduino driver's
  // default FLASH storage would rewrite its separate Wi-Fi record during
  // provisioning/reconnect cycles and unnecessarily wear NVS.
  WiFi.persistent(false);
  _wifiEventHandlerId = WiFi.onEvent(
      [this](arduino_event_id_t event, arduino_event_info_t info) {
        switch (event) {
          case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            _wifiEventBits.fetch_or(kEventSTAConnected);
            break;
          case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            _wifiEventBits.fetch_or(kEventSTAGotIP);
            break;
          case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            _eventDisconnectReason.store(info.wifi_sta_disconnected.reason);
            if (isCredentialFailureReason(
                    info.wifi_sta_disconnected.reason)) {
              // Preserve a terminal credential reason even if a later
              // disconnect is coalesced before process() drains the flags.
              _eventCredentialFailureReason.store(
                  info.wifi_sta_disconnected.reason);
            }
            _wifiEventBits.fetch_or(kEventSTADisconnected);
            break;
          default:
            break;
        }
      });

  if (_wifiEventHandlerId == 0) {
    activeWiFiOwner = nullptr;
    setError("Unable to register the Wi-Fi event handler");
    return false;
  }

  // The library owns reconnect timing. Leaving the core policy enabled would
  // create a second, uncoordinated reconnect path from the event task.
  if (!WiFi.setAutoReconnect(false)) {
    WiFi.removeEvent(_wifiEventHandlerId);
    _wifiEventHandlerId = 0;
    activeWiFiOwner = nullptr;
    setError("Unable to disable the Arduino Wi-Fi reconnect policy");
    return false;
  }
  return true;
}

bool ESP32WiFiPortal::connectSaved(uint32_t timeoutMs) {
  if (!ensureWiFiEventHandler()) {
    _state = State::Failed;
    return false;
  }
  cancelAutoReconnect(true);
  if (_portalActive) {
    stopConfigPortal();
  }

  if (!ensureCredentialCache()) {
    if (_credentialCacheStatus == CredentialCacheStatus::Unavailable) {
      setError("Saved Wi-Fi credentials are temporarily unavailable");
      _state = State::Failed;
      scheduleAutoReconnect(_maxRetryIntervalMs);
    } else if (_credentialCacheStatus == CredentialCacheStatus::Corrupt) {
      setError("Saved Wi-Fi credential record is corrupt");
      _state = State::Failed;
    } else {
      setError("No saved Wi-Fi credentials");
      _state = State::Failed;
    }
    return false;
  }

  const bool connected = connect(timeoutMs);
  if (!connected) scheduleSavedConnectionRecovery();
  return connected;
}

bool ESP32WiFiPortal::autoConnect(const char* apSSID,
                                  const char* apPassword,
                                  uint32_t connectTimeoutMs,
                                  uint32_t portalTimeoutMs) {
  if (connectSaved(connectTimeoutMs)) {
    return true;
  }
  return startConfigPortal(apSSID, apPassword, portalTimeoutMs);
}

bool ESP32WiFiPortal::startConfigPortal(const char* apSSID,
                                        const char* apPassword,
                                        uint32_t portalTimeoutMs) {
  if (!openPortal(apSSID, apPassword, portalTimeoutMs)) {
    return false;
  }

  while (_portalActive) {
    process();
    delay(2);
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

bool ESP32WiFiPortal::startConfigPortalAsync(const char* apSSID,
                                             const char* apPassword,
                                             uint32_t portalTimeoutMs) {
  return openPortal(apSSID, apPassword, portalTimeoutMs);
}

bool ESP32WiFiPortal::openPortal(const char* apSSID,
                                 const char* apPassword,
                                 uint32_t portalTimeoutMs) {
  if (!apSSID || strlen(apSSID) == 0 || strlen(apSSID) > 32) {
    setError("AP SSID must contain 1-32 bytes");
    return false;
  }
  if (!validAPPassword(apPassword)) {
    setError("AP password must be empty or 8-63 characters");
    return false;
  }

  const PortalNetworkValidationResult validation = validatePortalNetwork(
      ipv4ToUint32(_portalIP), ipv4ToUint32(_portalGateway),
      ipv4ToUint32(_portalSubnet));
  if (validation != PortalNetworkValidationResult::Valid) {
    setError(portalNetworkValidationMessage(validation));
    return false;
  }

  if (!ensureWiFiEventHandler()) {
    _state = State::Failed;
    return false;
  }
  // Load the last known-good credentials before a Portal candidate can use
  // the STA interface. They remain cached until a candidate is proven and
  // committed, or the application explicitly erases them.
  ensureCredentialCache();
  stopConfigPortal();
  cancelAutoReconnect(true);
  _lastError = "";
  _portalSSID = apSSID;
  _portalTimeoutMs = portalTimeoutMs;
  _portalStartedAt = millis();
  _connectPending = false;
  releaseSTAConnection();
  _portalRetriesUsed = 0;

  // Arduino-ESP32 2.x copies the configured STA hostname into the netif while
  // enabling the mode, so set it before mode()/begin().
  if (_hostname.length() > 0 && !WiFi.setHostname(_hostname.c_str())) {
    return failPortalStart("Failed to configure the Wi-Fi hostname");
  }
  if (!WiFi.mode(WIFI_AP_STA)) {
    return failPortalStart("Failed to enable ESP32 AP+STA mode");
  }
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
    return failPortalStart("Failed to select RAM-only Wi-Fi driver storage");
  }
  if (!WiFi.softAPConfig(_portalIP, _portalGateway, _portalSubnet)) {
    return failPortalStart(
        "WiFi.softAPConfig() failed for the requested portal IP/subnet");
  }

  if (_hostname.length() > 0) {
    if (!WiFi.softAPsetHostname(_hostname.c_str())) {
      return failPortalStart("Failed to configure the SoftAP hostname");
    }
  }

  bool apOk = false;
  if (apPassword && strlen(apPassword) > 0) {
    apOk = WiFi.softAP(apSSID, apPassword, _apChannel, _apHidden ? 1 : 0);
  } else {
    apOk = WiFi.softAP(apSSID, nullptr, _apChannel, _apHidden ? 1 : 0);
  }

  if (!apOk) {
    return failPortalStart("Failed to start ESP32 access point");
  }

  // softAPConfig() can fail incompletely on some core/IDF combinations. Read
  // back the active netif before DNS and HTTP bind to a different address.
  if (ipv4ToUint32(WiFi.softAPIP()) != ipv4ToUint32(_portalIP) ||
      ipv4ToUint32(WiFi.softAPSubnetMask()) != ipv4ToUint32(_portalSubnet)) {
    return failPortalStart(
        "SoftAP runtime IP/subnet does not match the portal configuration");
  }

  _redirectURL.remove(0);
  _redirectURL.reserve(24);
  _redirectURL += F("http://");
  appendIPAddress(_redirectURL, WiFi.softAPIP());
  _redirectURL += '/';

  _server.reset(new (std::nothrow) WebServer(kHttpPort));
  if (!_server) {
    return failPortalStart("Unable to allocate captive portal WebServer");
  }
  configureRoutes();
  _server->begin();

  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (!_dns.start(kDnsPort, "*", WiFi.softAPIP())) {
    return failPortalStart("Failed to start captive portal DNS");
  }

  _portalActive = true;
  _state = State::Portal;
  if (_loggingEnabled) {
    Serial.print(F("[EWP] Portal started: http://"));
    Serial.println(_portalIP);
  }
  invoke(_onPortalStarted);
  return true;
}

bool ESP32WiFiPortal::failPortalStart(const char* message) {
  _portalActive = false;
  clearPendingConnection(true);

  if (_server) {
    _server->stop();
    _server.reset();
  }
  _dns.stop();
  resetScan(true);
  bool softAPStopped = WiFi.softAPdisconnect(true);
  if (!softAPStopped) {
    // Some core/IDF combinations can fail while clearing the AP config. A
    // direct mode transition is a bounded fallback that still preserves STA.
    softAPStopped = WiFi.mode(WIFI_STA);
  }

  _portalTimeoutMs = 0;
  _portalStartedAt = 0;
  _responseBuffer = String();
  _scanSSID = String();
  _scanCompareSSID = String();
  _redirectURL = String();
  _scanNetworkIdentities.reset();
  _scanNetworkIdentityCapacity = 0;

  String failureMessage(message);
  if (!softAPStopped) {
    failureMessage += "; failed to disable the SoftAP interface";
  }
  setError(failureMessage);
  _state = WiFi.status() == WL_CONNECTED ? State::Connected : State::Failed;
  scheduleSavedConnectionRecovery();
  return false;
}

void ESP32WiFiPortal::configureRoutes() {
  if (!_server) return;

  _server->on("/", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/wifi", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/scan", HTTP_GET, [this]() { handleScan(); });
  _server->on("/save", HTTP_POST, [this]() { handleSave(); });
  _server->on("/status", HTTP_GET, [this]() { handleStatus(); });
  _server->on("/properties", HTTP_GET, [this]() { handleProperties(); });
  _server->on("/reset", HTTP_POST, [this]() { handleReset(); });

  // Common captive portal probes used by Android, Apple and Windows.
  _server->on("/generate_204", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/gen_204", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/hotspot-detect.html", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/library/test/success.html", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/connecttest.txt", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/ncsi.txt", HTTP_ANY, [this]() { handleCaptiveProbe(); });
  _server->on("/fwlink", HTTP_ANY, [this]() { handleCaptiveProbe(); });

  _server->onNotFound([this]() { handleNotFound(); });
}

void ESP32WiFiPortal::handleRoot() {
  if (!_server) return;
  _server->send_P(200, "text/html; charset=utf-8", EWP_PORTAL_HTML);
}

void ESP32WiFiPortal::processScan() {
  if (_scanState != ScanState::Scanning) return;

  const int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING &&
      millis() - _scanStartedAt < kScanTimeoutMs) {
    return;
  }

  if (result >= 0) {
    _scanResultCount = result;
    _scanState = ScanState::Ready;
    return;
  }

  // The Arduino core normally times out first. This explicit upper bound also
  // prevents an unusual driver state from leaving the Portal stuck polling.
  if (result == WIFI_SCAN_RUNNING) esp_wifi_scan_stop();
  WiFi.scanDelete();
  _scanResultCount = 0;
  _scanState = ScanState::Failed;
}

void ESP32WiFiPortal::resetScan(bool cancelActiveScan) {
  if (_scanState == ScanState::Idle) return;
  if (cancelActiveScan && _scanState == ScanState::Scanning) {
    esp_wifi_scan_stop();
  }
  WiFi.scanDelete();
  _scanState = ScanState::Idle;
  _scanStartedAt = 0;
  _scanResultCount = 0;
}

void ESP32WiFiPortal::handleScan() {
  if (!_server) return;

  if (_connectPending || _connectAttemptActive) {
    _server->sendHeader("Cache-Control", "no-store");
    _server->send(409, "application/json; charset=utf-8",
                  "{\"networks\":[],\"error\":\"Wi-Fi connection in progress\"}");
    return;
  }

  processScan();
  if (_scanState == ScanState::Idle) {
    const int16_t result = WiFi.scanNetworks(true, true);
    _scanStartedAt = millis();
    if (result == WIFI_SCAN_RUNNING) {
      _scanState = ScanState::Scanning;
    } else if (result >= 0) {
      _scanResultCount = result;
      _scanState = ScanState::Ready;
    } else {
      WiFi.scanDelete();
      _scanState = ScanState::Failed;
    }
  }

  if (_scanState == ScanState::Scanning) {
    _server->sendHeader("Cache-Control", "no-store");
    _server->sendHeader("Retry-After", "1");
    _server->send(202, "application/json; charset=utf-8",
                  "{\"scanning\":true}");
    return;
  }

  if (_scanState == ScanState::Failed) {
    _server->sendHeader("Cache-Control", "no-store");
    _server->send(503, "application/json; charset=utf-8",
                  "{\"networks\":[],\"error\":\"Wi-Fi scan failed\"}");
    resetScan(false);
    return;
  }

  const int count = _scanResultCount;

  if (static_cast<size_t>(count) > _scanNetworkIdentityCapacity) {
    std::unique_ptr<ScanNetworkIdentity[]> identities(
        new (std::nothrow) ScanNetworkIdentity[count]);
    if (identities) {
      _scanNetworkIdentities = std::move(identities);
      _scanNetworkIdentityCapacity = static_cast<size_t>(count);
    }
  }

  const bool hasIdentityBuffer =
      static_cast<size_t>(count) <= _scanNetworkIdentityCapacity;
  size_t uniqueCount = 0;
  bool wroteNetwork = false;

  _responseBuffer.remove(0);
  _responseBuffer.reserve(96 + static_cast<size_t>(count) * 80);
  _responseBuffer = F("{\"networks\":[");
  _scanSSID.reserve(32);
  _scanCompareSSID.reserve(32);

  // Emit unique SSIDs only, strongest first (Arduino scan is normally RSSI sorted).
  for (int i = 0; i < count; ++i) {
    _scanSSID = WiFi.SSID(i);
    if (_scanSSID.length() == 0) continue;

    bool duplicate = false;
    if (hasIdentityBuffer) {
      const uint32_t hash = hashSSID(_scanSSID);
      for (size_t j = 0; j < uniqueCount; ++j) {
        if (_scanNetworkIdentities[j].hash != hash) continue;
        _scanCompareSSID = WiFi.SSID(_scanNetworkIdentities[j].index);
        if (_scanCompareSSID == _scanSSID) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        _scanNetworkIdentities[uniqueCount].hash = hash;
        _scanNetworkIdentities[uniqueCount].index = i;
        ++uniqueCount;
      }
    } else {
      // Preserve exact duplicate filtering if the small temporary identity
      // buffer cannot be allocated.
      for (int j = 0; j < i; ++j) {
        _scanCompareSSID = WiFi.SSID(j);
        if (_scanCompareSSID == _scanSSID) {
          duplicate = true;
          break;
        }
      }
    }
    if (duplicate) continue;

    if (wroteNetwork) _responseBuffer += ',';
    wroteNetwork = true;

    const bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    _responseBuffer += F("{\"ssid\":\"");
    appendJsonEscaped(_responseBuffer, _scanSSID);
    _responseBuffer += F("\",\"rssi\":");
    _responseBuffer += WiFi.RSSI(i);
    _responseBuffer += F(",\"open\":");
    _responseBuffer += open ? F("true") : F("false");
    _responseBuffer += '}';
  }

  _responseBuffer += F("]}");
  resetScan(false);

  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "application/json; charset=utf-8", _responseBuffer);
}

void ESP32WiFiPortal::handleSave() {
  if (!_server) return;

  if (_connectPending || _connectAttemptActive) {
    _server->send(409, "text/plain; charset=utf-8", "A Wi-Fi connection attempt is already running");
    return;
  }

  // Move request values directly into reusable candidate buffers instead of
  // creating another pair of short-lived credential Strings.
  _pendingSSID = _server->arg("ssid");
  _pendingPassword = _server->arg("password");

  if (!validSTACredentials(_pendingSSID, _pendingPassword)) {
    _pendingSSID.remove(0);
    _pendingPassword.remove(0);
    _server->send(400, "text/plain; charset=utf-8",
                  "SSID must be 1-32 bytes; password must be empty, 8-63 bytes, or a 64-digit hexadecimal PSK");
    return;
  }

  // A valid connection request takes priority over a scan started by another
  // Portal client. The normal UI only submits after scan results are ready.
  resetScan(true);

  _connectPending = true;
  _connectPendingAt = millis();
  _connectPendingDelayMs = 350;
  _portalRetriesUsed = 0;
  _attemptTerminalFailure = false;
  _lastError = "";

  _server->send_P(200, "text/html; charset=utf-8", EWP_CONNECTING_HTML);
}

void ESP32WiFiPortal::handleStatus() {
  if (!_server) return;
  const bool candidateInProgress =
      _connectPending ||
      (_connectAttemptActive && _connectionOwner == ConnectionOwner::Portal);
  const bool connected = isConnected();
  _responseBuffer.remove(0);
  _responseBuffer.reserve(96 + _lastError.length());
  _responseBuffer = F("{\"connected\":");
  _responseBuffer += connected && !candidateInProgress ? F("true") : F("false");
  _responseBuffer += F(",\"portal\":");
  _responseBuffer += isPortalActive() ? F("true") : F("false");
  _responseBuffer += F(",\"ip\":\"");
  appendIPAddress(_responseBuffer,
                  connected ? WiFi.localIP() : WiFi.softAPIP());
  _responseBuffer += F("\",\"error\":\"");
  appendJsonEscaped(_responseBuffer, _lastError);
  _responseBuffer += F("\"}");
  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "application/json; charset=utf-8", _responseBuffer);
}

void ESP32WiFiPortal::handleProperties() {
  if (!_server) return;

  const bool staConnected = WiFi.status() == WL_CONNECTED;
  uint8_t apMAC[6] = {};
  uint8_t staMAC[6] = {};
  WiFi.softAPmacAddress(apMAC);
  WiFi.macAddress(staMAC);

  _responseBuffer.remove(0);
  _responseBuffer.reserve(480 + _portalSSID.length());
  _responseBuffer = F("{\"ssid\":\"");
  appendJsonEscaped(_responseBuffer, _portalSSID);
  _responseBuffer += F("\",\"softap\":{\"ip\":\"");
  appendIPAddress(_responseBuffer, WiFi.softAPIP());
  _responseBuffer += F("\",\"gateway\":\"");
  // Arduino-ESP32 exposes the runtime SoftAP IP/subnet, but no matching
  // SoftAP gateway getter. This is the exact value configured by openPortal().
  appendIPAddress(_responseBuffer, _portalGateway);
  _responseBuffer += F("\",\"subnet\":\"");
  appendIPAddress(_responseBuffer, WiFi.softAPSubnetMask());
  _responseBuffer += F("\"},\"sta\":{\"connected\":");
  _responseBuffer += staConnected ? F("true") : F("false");
  _responseBuffer += F(",\"ip\":\"");
  if (staConnected) appendIPAddress(_responseBuffer, WiFi.localIP());
  _responseBuffer += F("\",\"gateway\":\"");
  if (staConnected) appendIPAddress(_responseBuffer, WiFi.gatewayIP());
  _responseBuffer += F("\",\"subnet\":\"");
  if (staConnected) appendIPAddress(_responseBuffer, WiFi.subnetMask());
  _responseBuffer += F("\"},\"mac\":{\"ap\":\"");
  appendMACAddress(_responseBuffer, apMAC);
  _responseBuffer += F("\",\"sta\":\"");
  appendMACAddress(_responseBuffer, staMAC);
  _responseBuffer += F("\"},\"chipId\":\"");
  appendHex64(_responseBuffer, ESP.getEfuseMac());
  _responseBuffer += F("\",\"cpuMHz\":");
  _responseBuffer += ESP.getCpuFreqMHz();
  _responseBuffer += F(",\"flashSize\":");
  _responseBuffer += ESP.getFlashChipSize();
  _responseBuffer += F(",\"flashSpeed\":");
  _responseBuffer += ESP.getFlashChipSpeed();
  _responseBuffer += F(",\"freeHeap\":");
  _responseBuffer += ESP.getFreeHeap();
  _responseBuffer += '}';

  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "application/json; charset=utf-8", _responseBuffer);
}

void ESP32WiFiPortal::handleReset() {
  if (!_server) return;

  _server->sendHeader("Cache-Control", "no-store");
  _server->send(202, "application/json; charset=utf-8",
                "{\"restarting\":true}");

  // Keep the first timestamp so repeated POSTs cannot postpone the reboot.
  if (!_restartPending) {
    _restartRequestedAt = millis();
    _restartPending = true;
  }
}

void ESP32WiFiPortal::handleCaptiveProbe() {
  if (!_server) return;
  _server->sendHeader("Location", _redirectURL, true);
  _server->send(302, "text/plain", "");
}

void ESP32WiFiPortal::handleNotFound() {
  if (!_server) return;
  handleCaptiveProbe();
}

void ESP32WiFiPortal::process() {
  processWiFiEvents();

  if (_restartPending &&
      millis() - _restartRequestedAt >= kRestartDelayMs) {
    _restartPending = false;
    ESP.restart();
    return;
  }

  if (_portalActive) {
    processScan();
    _dns.processNextRequest();
    if (_server) _server->handleClient();

    if (portalTimedOut()) {
      setError("Configuration portal timed out");
      log(F("[EWP] Portal timeout"));
      stopConfigPortal();
      if (WiFi.status() == WL_CONNECTED) {
        _state = State::Connected;
      } else if (_reconnectScheduled ||
                 (_connectAttemptActive &&
                  _connectionOwner == ConnectionOwner::Reconnect)) {
        _state = State::Connecting;
      } else {
        _state = State::Failed;
      }
      return;
    }

    if (_connectPending && !_connectAttemptActive &&
        millis() - _connectPendingAt >= _connectPendingDelayMs) {
      beginPendingConnection();
      return;
    }

    if (_connectAttemptActive && _connectionOwner == ConnectionOwner::Portal) {
      if (!advanceSTAConnection()) {
        clearPendingConnection(false);
        _state = State::Portal;
        return;
      }

      if (_connectionPhase != ConnectionPhase::Connecting) return;

      if (WiFi.status() == WL_CONNECTED) {
        if (!saveCredentials(_pendingSSID, _pendingPassword)) {
          clearPendingConnection(true);
          _state = State::Portal;
          setError("Connected, but unable to save Wi-Fi settings");
          return;
        }
        clearPendingConnection(false);
        _state = State::Connected;
        invoke(_onCredentialsSaved);
        invoke(_onConnected);
        stopConfigPortal();
        _state = State::Connected;
        return;
      }

      if (_attemptTerminalFailure) {
        failPendingConnection(true);
      } else if (millis() - _connectAttemptAt >= _connectTimeoutMs) {
        failPendingConnection(false);
      }
    }
  }

  processAutoReconnect();
}

void ESP32WiFiPortal::beginPendingConnection() {
  if (!_connectPending || _connectAttemptActive) return;

  _connectPending = false;
  _lastError = "";
  if (!beginSTAConnection(ConnectionOwner::Portal)) {
    clearPendingConnection(false);
    _state = State::Portal;
  }
}

void ESP32WiFiPortal::clearPendingConnection(bool disconnectSTA) {
  const bool ownsSTA = _connectionOwner == ConnectionOwner::Portal;
  if (disconnectSTA && ownsSTA && _connectAttemptActive) {
    cancelSTAConnection();
  } else if (ownsSTA) {
    releaseSTAConnection();
    _staDisconnected = WiFi.status() != WL_CONNECTED;
  }

  _connectPending = false;
  _connectPendingAt = 0;
  _connectPendingDelayMs = 350;
  _portalRetriesUsed = 0;
  // Keep these small buffers for the next Portal attempt to avoid repeatedly
  // allocating and freeing credential-sized heap blocks.
  _pendingSSID.remove(0);
  _pendingPassword.remove(0);
}

void ESP32WiFiPortal::failPendingConnection(bool terminalFailure) {
  cancelSTAConnection();

  if (!terminalFailure && _portalRetriesUsed < _maxConnectionRetries) {
    ++_portalRetriesUsed;
    _connectPending = true;
    _connectPendingAt = millis();
    _connectPendingDelayMs = retryDelay(_portalRetriesUsed);
    _state = State::Portal;
    _lastError = "";
    if (_loggingEnabled) {
      Serial.print(F("[EWP] Retry "));
      Serial.print(_portalRetriesUsed);
      Serial.print('/');
      Serial.println(_maxConnectionRetries);
    }
    return;
  }

  clearPendingConnection(false);
  _state = State::Portal;
  setError(terminalFailure
               ? "Wi-Fi authentication failed. Check the password and try again."
               : "Unable to connect. Check the SSID/password and try again.");
}

bool ESP32WiFiPortal::beginSTAConnection(ConnectionOwner owner) {
  if (_connectAttemptActive || owner == ConnectionOwner::None) {
    setError("A Wi-Fi connection attempt is already running");
    return false;
  }

  if (!ensureWiFiEventHandler()) return false;

  processWiFiEvents();
  if (_hostname.length() > 0 && !WiFi.setHostname(_hostname.c_str())) {
    setError("Failed to configure the Wi-Fi hostname");
    return false;
  }
  if (!WiFi.mode(_portalActive ? WIFI_AP_STA : WIFI_STA)) {
    setError("Failed to enable the required ESP32 Wi-Fi mode");
    return false;
  }
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
    setError("Failed to select RAM-only Wi-Fi driver storage");
    return false;
  }
  if (!WiFi.setAutoReconnect(false)) {
    setError("Unable to disable the Arduino Wi-Fi reconnect policy");
    return false;
  }
  if (_hostname.length() > 0 && _portalActive &&
      !WiFi.softAPsetHostname(_hostname.c_str())) {
    setError("Failed to configure the SoftAP hostname");
    return false;
  }

  _connectionOwner = owner;
  _connectionPhase = ConnectionPhase::Settling;
  _connectAttemptActive = true;
  _connectAttemptAt = 0;
  _connectionPhaseAt = millis();
  _connectionSettleDelayMs = 0;
  _attemptTerminalFailure = false;

  // cancelSTAConnection() already leaves STA clean. Avoid a second disconnect
  // before the next retry, but remain conservative for the first attempt or a
  // currently connected interface.
  if (!_staDisconnected || WiFi.status() == WL_CONNECTED) {
    const bool wasConnected = WiFi.status() == WL_CONNECTED;
    const bool disconnected = WiFi.disconnect(false, false);
    if (!disconnected && wasConnected && WiFi.status() == WL_CONNECTED) {
      releaseSTAConnection();
      setError("Failed to disconnect the current STA connection");
      return false;
    }
    _staDisconnected = true;
    _connectionSettleDelayMs = kSTADisconnectSettleMs;
  }
  return true;
}

bool ESP32WiFiPortal::advanceSTAConnection() {
  if (!_connectAttemptActive) return false;
  if (_connectionPhase == ConnectionPhase::Connecting) return true;
  if (_connectionPhase != ConnectionPhase::Settling) return false;
  if (millis() - _connectionPhaseAt < _connectionSettleDelayMs) return true;

  if (!applySTAConfig()) {
    cancelSTAConnection();
    setError("Failed to apply STA IP/DNS configuration");
    return false;
  }

  const String* ssid = nullptr;
  const String* password = nullptr;
  if (_connectionOwner == ConnectionOwner::Portal) {
    ssid = &_pendingSSID;
    password = &_pendingPassword;
  } else if (_connectionOwner == ConnectionOwner::Blocking ||
             _connectionOwner == ConnectionOwner::Reconnect) {
    ssid = &_savedSSID;
    password = &_savedPassword;
  }
  if (!ssid || ssid->length() == 0 || !password) {
    cancelSTAConnection();
    setError("Wi-Fi credentials are unavailable");
    return false;
  }

  _connectionPhase = ConnectionPhase::Connecting;
  _connectAttemptAt = millis();
  _attemptTerminalFailure = false;
  log(F("[EWP] Connect"));

  _staDisconnected = false;
  const wl_status_t result = WiFi.begin(ssid->c_str(), password->c_str());
  if (result == WL_CONNECT_FAILED) {
    cancelSTAConnection();
    setError("Unable to start Wi-Fi connection");
    return false;
  }
  return true;
}

bool ESP32WiFiPortal::applySTAConfig() {
  if (_staStaticIPEnabled) {
    return WiFi.config(_staIP, _staGateway, _staSubnet,
                       _staPrimaryDNS, _staSecondaryDNS);
  }

  // A zero local address restarts DHCP in Arduino-ESP32. Applying it before
  // every managed attempt also cleanly exits a previous static configuration.
  return WiFi.config(IPAddress(), IPAddress(), IPAddress(),
                     IPAddress(), IPAddress());
}

void ESP32WiFiPortal::releaseSTAConnection() {
  _connectAttemptActive = false;
  _connectionOwner = ConnectionOwner::None;
  _connectionPhase = ConnectionPhase::Idle;
  _connectAttemptAt = 0;
  _connectionPhaseAt = 0;
  _connectionSettleDelayMs = 0;
  _attemptTerminalFailure = false;
}

void ESP32WiFiPortal::cancelSTAConnection() {
  const bool disconnectSTA =
      _connectAttemptActive &&
      _connectionPhase == ConnectionPhase::Connecting &&
      !_staDisconnected;
  releaseSTAConnection();
  if (disconnectSTA) {
    const bool disconnected = WiFi.disconnect(false, false);
    _staDisconnected = WiFi.status() != WL_CONNECTED;
    if (!disconnected && !_staDisconnected) {
      setError("Failed to cancel the STA connection attempt");
    }
  }
}

void ESP32WiFiPortal::processWiFiEvents() {
  const uint32_t events = _wifiEventBits.exchange(0);
  if (events == 0) return;

  const bool connectedNow = WiFi.status() == WL_CONNECTED;
  if ((events & kEventSTAConnected) != 0 && _loggingEnabled) {
    Serial.println(F("[EWP] STA connected"));
  }
  if ((events & kEventSTAConnected) != 0) _staDisconnected = false;

  if ((events & kEventSTAGotIP) != 0 && connectedNow) {
    _staDisconnected = false;
    if (_loggingEnabled) {
      Serial.print(F("[EWP] Got IP: "));
      Serial.println(WiFi.localIP());
    }
    if (_connectionOwner == ConnectionOwner::None && !_portalActive) {
      _reconnectScheduled = false;
      _reconnectRetriesUsed = 0;
      _state = State::Connected;
    }
  }

  if ((events & kEventSTADisconnected) == 0) return;

  uint8_t reason = static_cast<uint8_t>(_eventDisconnectReason.load());
  const uint8_t credentialFailureReason = static_cast<uint8_t>(
      _eventCredentialFailureReason.exchange(0));
  if (reason == 0) reason = WIFI_REASON_UNSPECIFIED;
  _lastDisconnectReason = reason;
  logDisconnect(reason);
  if (!_connectAttemptActive && !connectedNow) _staDisconnected = true;
  if (connectedNow || reason == WIFI_REASON_ASSOC_LEAVE) return;

  if (_connectAttemptActive) {
    // Saved credentials may see AUTH_FAIL during weak signal, AP overload, or
    // a router restart. Only an unproven Portal/initial blocking candidate is
    // allowed to treat a clear credential rejection as terminal. Reconnect
    // always remains recoverable through its bounded cooldown policy.
    if (_connectionPhase == ConnectionPhase::Connecting &&
        _connectionOwner != ConnectionOwner::Reconnect) {
      _attemptTerminalFailure =
          _attemptTerminalFailure || credentialFailureReason != 0 ||
          isCredentialFailureReason(reason);
    }
    return;
  }

  if (_autoReconnectEnabled && !_portalActive && _state == State::Connected) {
    _reconnectRetriesUsed = 0;
    scheduleAutoReconnect(_retryIntervalMs);
  }
}

void ESP32WiFiPortal::processAutoReconnect() {
  if (!_autoReconnectEnabled || _portalActive) return;

  if (_connectAttemptActive && _connectionOwner == ConnectionOwner::Reconnect) {
    if (!advanceSTAConnection()) {
      scheduleNextAutoReconnect();
      return;
    }
    if (_connectionPhase != ConnectionPhase::Connecting) return;

    if (WiFi.status() == WL_CONNECTED) {
      releaseSTAConnection();
      _staDisconnected = false;
      _reconnectScheduled = false;
      _reconnectRetriesUsed = 0;
      _lastError = "";
      _state = State::Connected;
      invoke(_onConnected);
      return;
    }

    if (millis() - _connectAttemptAt < _connectTimeoutMs) {
      return;
    }

    cancelSTAConnection();
    scheduleNextAutoReconnect();
    return;
  }

  if (!_reconnectScheduled || _connectAttemptActive ||
      millis() - _reconnectScheduledAt < _reconnectDelayMs) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _reconnectScheduled = false;
    _reconnectRetriesUsed = 0;
    _staDisconnected = false;
    _state = State::Connected;
    return;
  }

  if (!ensureCredentialCache()) {
    if (_credentialCacheStatus != CredentialCacheStatus::Unavailable) {
      _reconnectScheduled = false;
      setError(_credentialCacheStatus == CredentialCacheStatus::Corrupt
                   ? "Auto reconnect rejected a corrupt credential record"
                   : "Auto reconnect requires saved Wi-Fi credentials");
      _state = State::Failed;
    } else {
      setError("Saved Wi-Fi credentials are temporarily unavailable");
      scheduleAutoReconnect(_maxRetryIntervalMs);
    }
    return;
  }

  _reconnectScheduled = false;
  if (_loggingEnabled) {
    if (_reconnectRetriesUsed == 0) {
      Serial.println(F("[EWP] Reconnect"));
    } else {
      Serial.print(F("[EWP] Retry "));
      Serial.print(_reconnectRetriesUsed);
      Serial.print('/');
      Serial.println(_maxConnectionRetries);
    }
  }

  _state = State::Connecting;
  if (!beginSTAConnection(ConnectionOwner::Reconnect)) {
    scheduleNextAutoReconnect();
  }
}

void ESP32WiFiPortal::scheduleAutoReconnect(uint32_t delayMs) {
  if (!_autoReconnectEnabled || _portalActive) return;
  _reconnectScheduled = true;
  _reconnectScheduledAt = millis();
  _reconnectDelayMs = delayMs;
  _state = State::Connecting;
}

void ESP32WiFiPortal::scheduleNextAutoReconnect() {
  if (_reconnectRetriesUsed < _maxConnectionRetries) {
    ++_reconnectRetriesUsed;
    scheduleAutoReconnect(retryDelay(_reconnectRetriesUsed));
    return;
  }

  _reconnectRetriesUsed = 0;
  scheduleAutoReconnect(_maxRetryIntervalMs);
  log(F("[EWP] Reconnect cooldown"));
}

bool ESP32WiFiPortal::scheduleSavedConnectionRecovery() {
  if (!_autoReconnectEnabled || _portalActive || _connectAttemptActive ||
      WiFi.status() == WL_CONNECTED) {
    return false;
  }

  const bool hasCredentials = ensureCredentialCache();
  if (!hasCredentials &&
      _credentialCacheStatus != CredentialCacheStatus::Unavailable) {
    return false;
  }

  _reconnectRetriesUsed = 0;
  scheduleAutoReconnect(hasCredentials ? _retryIntervalMs
                                       : _maxRetryIntervalMs);
  log(F("[EWP] Restoring saved Wi-Fi"));
  return true;
}

void ESP32WiFiPortal::cancelAutoReconnect(bool disconnectSTA) {
  _reconnectScheduled = false;
  _reconnectRetriesUsed = 0;
  _reconnectScheduledAt = 0;
  _reconnectDelayMs = 0;
  if (_connectionOwner == ConnectionOwner::Reconnect) {
    if (disconnectSTA) {
      cancelSTAConnection();
    } else {
      releaseSTAConnection();
      _staDisconnected = WiFi.status() != WL_CONNECTED;
    }
  }
}

uint32_t ESP32WiFiPortal::retryDelay(uint8_t retryNumber) const {
  uint32_t delayMs = _retryIntervalMs;
  for (uint8_t i = 1; i < retryNumber && delayMs < _maxRetryIntervalMs; ++i) {
    if (delayMs > _maxRetryIntervalMs / 2) {
      delayMs = _maxRetryIntervalMs;
    } else {
      delayMs *= 2;
    }
  }
  return delayMs > _maxRetryIntervalMs ? _maxRetryIntervalMs : delayMs;
}

bool ESP32WiFiPortal::connect(uint32_t timeoutMs) {
  if (_savedSSID.length() == 0) {
    setError("SSID cannot be empty");
    _state = State::Failed;
    return false;
  }

  if (_portalActive) {
    stopConfigPortal();
  }

  if (!ensureWiFiEventHandler()) {
    _state = State::Failed;
    return false;
  }
  cancelAutoReconnect(true);
  if (timeoutMs == 0) {
    timeoutMs = kDefaultConnectTimeoutMs;
    log(F("[EWP] Connect timeout 0 normalized to 15000 ms"));
  }
  _state = State::Connecting;
  _lastError = "";
  uint8_t retriesUsed = 0;

  while (true) {
    if (!beginSTAConnection(ConnectionOwner::Blocking)) {
      _state = State::Failed;
      return false;
    }

    bool setupFailed = false;
    while (true) {
      processWiFiEvents();
      if (!advanceSTAConnection()) {
        setupFailed = true;
        break;
      }
      if (_connectionPhase != ConnectionPhase::Connecting) {
        delay(1);
        yield();
        continue;
      }
      if (WiFi.status() == WL_CONNECTED) break;
      if (_attemptTerminalFailure ||
          millis() - _connectAttemptAt >= timeoutMs) {
        break;
      }
      delay(10);
      yield();
    }

    if (setupFailed) {
      _state = State::Failed;
      return false;
    }

    if (WiFi.status() == WL_CONNECTED) {
      releaseSTAConnection();
      _staDisconnected = false;
      _state = State::Connected;
      invoke(_onConnected);
      return true;
    }

    const bool terminalFailure = _attemptTerminalFailure;
    cancelSTAConnection();
    if (terminalFailure || retriesUsed >= _maxConnectionRetries) {
      setError(terminalFailure ? "Wi-Fi authentication failed"
                               : "Wi-Fi connection timed out");
      _state = State::Failed;
      return false;
    }

    ++retriesUsed;
    const uint32_t waitMs = retryDelay(retriesUsed);
    if (_loggingEnabled) {
      Serial.print(F("[EWP] Retry "));
      Serial.print(retriesUsed);
      Serial.print('/');
      Serial.println(_maxConnectionRetries);
    }
    const uint32_t waitStartedAt = millis();
    while (millis() - waitStartedAt < waitMs) {
      processWiFiEvents();
      delay(5);
      yield();
    }
  }
}

void ESP32WiFiPortal::stopConfigPortal() {
  const bool wasPortalActive = _portalActive;
  _portalActive = false;
  clearPendingConnection(true);

  if (_server) {
    _server->stop();
    _server.reset();
  }
  _dns.stop();
  resetScan(true);

  bool softAPStopped = true;
  if (wasPortalActive) {
    softAPStopped = WiFi.softAPdisconnect(true);
    if (!softAPStopped) {
      softAPStopped = WiFi.mode(WIFI_STA);
    }
    log(F("[EWP] Portal stopped"));
  }

  _portalTimeoutMs = 0;
  _portalStartedAt = 0;
  _responseBuffer = String();
  _scanSSID = String();
  _scanCompareSSID = String();
  _redirectURL = String();
  _scanNetworkIdentities.reset();
  _scanNetworkIdentityCapacity = 0;

  if (_state == State::Portal) {
    _state = WiFi.status() == WL_CONNECTED ? State::Connected : State::Idle;
  }

  if (!softAPStopped) {
    setError("Failed to disable the SoftAP interface");
    _state = State::Failed;
  }

  if (wasPortalActive && WiFi.status() != WL_CONNECTED) {
    scheduleSavedConnectionRecovery();
  }
}

bool ESP32WiFiPortal::saveCredentials(const String& ssid, const String& password) {
  if (!validSTACredentials(ssid, password)) {
    setError("Invalid Wi-Fi credentials");
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    setError("Unable to open NVS namespace");
    return false;
  }

  // An interrupted erase marker invalidates every older credential record.
  // Keep the marker until the replacement primary has survived exact
  // read-back, so reset during provisioning cannot resurrect erased data.
  const bool erasePending = prefs.isKey(kPrefsCredentialEraseMarker);
  if (erasePending && !removeStoredCredentials(prefs)) {
    prefs.end();
    setError("Unable to finish the pending credential erase");
    clearCredentialCache(CredentialCacheStatus::Unavailable);
    return false;
  }

  bool saved = writeCredentialRecord(prefs, ssid, password);
  if (saved && erasePending) {
    saved = prefs.remove(kPrefsCredentialEraseMarker);
  }
  if (saved) {
    // Legacy keys are kept until the new record has survived an exact
    // read-back and CRC validation. A reset at any earlier point can safely
    // retry migration from the complete legacy pair.
    if (prefs.isKey(kPrefsSSID)) prefs.remove(kPrefsSSID);
    if (prefs.isKey(kPrefsPassword)) prefs.remove(kPrefsPassword);
  }
  prefs.end();

  if (saved) {
    _savedSSID = ssid;
    _savedPassword = password;
    _credentialCacheStatus = CredentialCacheStatus::Valid;
  } else {
    setError("Unable to commit the Wi-Fi credential transaction");
  }
  return saved;
}

bool ESP32WiFiPortal::ensureCredentialCache() {
  if (_credentialCacheStatus == CredentialCacheStatus::Valid) return true;
  if (_credentialCacheStatus == CredentialCacheStatus::NotFound ||
      _credentialCacheStatus == CredentialCacheStatus::Corrupt) {
    return false;
  }

  Preferences prefs;
  // Read-write mode lets Preferences open a fresh namespace, so an empty
  // device is distinguishable from an NVS-open failure. This path runs only
  // while the object cache is unresolved, not on every reconnect attempt.
  if (!prefs.begin(kPrefsNamespace, false)) {
    clearCredentialCache(CredentialCacheStatus::Unavailable);
    return false;
  }

  if (prefs.isKey(kPrefsCredentialEraseMarker)) {
    // Marker presence is intentionally fail-closed, including a short marker
    // left by power loss. Old primary, backup, and legacy keys must never be
    // considered once erase has started.
    const bool removed = removeStoredCredentials(prefs);
    const bool markerRemoved =
        removed && prefs.remove(kPrefsCredentialEraseMarker);
    prefs.end();
    clearCredentialCache(removed && markerRemoved
                             ? CredentialCacheStatus::NotFound
                             : CredentialCacheStatus::Unavailable);
    return false;
  }

  String ssid;
  String password;
  const CredentialCacheStatus blobStatus =
      readCredentialBlob(prefs, kPrefsCredential, ssid, password);
  const bool staleBackup = prefs.isKey(kPrefsCredentialBackup);
  const bool staleLegacyKeys =
      prefs.isKey(kPrefsSSID) || prefs.isKey(kPrefsPassword);
  if (blobStatus == CredentialCacheStatus::Valid) {
    // A valid primary record is authoritative. Backup/legacy keys can remain
    // only when power was lost after the primary commit and before cleanup.
    if (staleBackup) prefs.remove(kPrefsCredentialBackup);
    if (staleLegacyKeys) {
      if (prefs.isKey(kPrefsSSID)) prefs.remove(kPrefsSSID);
      if (prefs.isKey(kPrefsPassword)) prefs.remove(kPrefsPassword);
    }
    prefs.end();
    _savedSSID = std::move(ssid);
    _savedPassword = std::move(password);
    _credentialCacheStatus = CredentialCacheStatus::Valid;
    return true;
  }

  String backupSSID;
  String backupPassword;
  const CredentialCacheStatus backupStatus = readCredentialBlob(
      prefs, kPrefsCredentialBackup, backupSSID, backupPassword);
  if (backupStatus == CredentialCacheStatus::Valid) {
    // An interrupted update may leave a partial primary. Use the verified old
    // value immediately and repair the primary opportunistically. A failed
    // repair leaves the backup intact for the next boot.
    writeCredentialRecord(prefs, backupSSID, backupPassword);
    prefs.end();
    _savedSSID = std::move(backupSSID);
    _savedPassword = std::move(backupPassword);
    _credentialCacheStatus = CredentialCacheStatus::Valid;
    return true;
  }

  // A valid legacy pair is also the recovery point for an interrupted first
  // blob write. A corrupt blob without a complete legacy pair is never used.
  const CredentialCacheStatus legacyStatus =
      readLegacyCredentials(prefs, ssid, password);

  if (legacyStatus == CredentialCacheStatus::Valid) {
    const bool migrated = writeCredentialRecord(prefs, ssid, password);
    if (migrated) {
      if (prefs.isKey(kPrefsSSID)) prefs.remove(kPrefsSSID);
      if (prefs.isKey(kPrefsPassword)) prefs.remove(kPrefsPassword);
    }
    prefs.end();
    if (!migrated) {
      clearCredentialCache(CredentialCacheStatus::Unavailable);
      return false;
    }

    _savedSSID = std::move(ssid);
    _savedPassword = std::move(password);
    _credentialCacheStatus = CredentialCacheStatus::Valid;
    return true;
  }

  prefs.end();
  clearCredentialCache(
      blobStatus == CredentialCacheStatus::Corrupt ||
              backupStatus == CredentialCacheStatus::Corrupt ||
              legacyStatus == CredentialCacheStatus::Corrupt
          ? CredentialCacheStatus::Corrupt
          : CredentialCacheStatus::NotFound);
  return false;
}

bool ESP32WiFiPortal::validSTACredentials(const String& ssid,
                                          const String& password) {
  const size_t ssidLength = ssid.length();
  const size_t passwordLength = password.length();
  if (ssidLength == 0 || ssidLength > 32 || passwordLength > 64 ||
      (passwordLength > 0 && passwordLength < 8)) {
    return false;
  }
  if (passwordLength == 64 && !isValidRawPSK(password)) return false;

  // WiFi.begin() consumes C strings, so embedded NUL bytes cannot be stored as
  // part of an exact credential value.
  for (size_t i = 0; i < ssidLength; ++i) {
    if (ssid[i] == '\0') return false;
  }
  for (size_t i = 0; i < passwordLength; ++i) {
    if (password[i] == '\0') return false;
  }
  return true;
}

bool ESP32WiFiPortal::isValidRawPSK(const String& password) {
  if (password.length() != 64) return false;
  for (size_t i = 0; i < password.length(); ++i) {
    const char character = password[i];
    const bool hexadecimal =
        (character >= '0' && character <= '9') ||
        (character >= 'a' && character <= 'f') ||
        (character >= 'A' && character <= 'F');
    if (!hexadecimal) return false;
  }
  return true;
}

uint32_t ESP32WiFiPortal::credentialCRC32(const uint8_t* data,
                                          size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

bool ESP32WiFiPortal::serializeCredentialRecord(const String& ssid,
                                                const String& password,
                                                uint8_t* record,
                                                size_t recordSize) {
  if (!record || recordSize != kCredentialRecordSize ||
      !validSTACredentials(ssid, password)) {
    return false;
  }

  memset(record, 0, recordSize);
  record[0] = static_cast<uint8_t>(kCredentialMagic);
  record[1] = static_cast<uint8_t>(kCredentialMagic >> 8);
  record[2] = static_cast<uint8_t>(kCredentialMagic >> 16);
  record[3] = static_cast<uint8_t>(kCredentialMagic >> 24);
  record[4] = static_cast<uint8_t>(kCredentialVersion);
  record[5] = static_cast<uint8_t>(kCredentialVersion >> 8);
  record[6] = static_cast<uint8_t>(kCredentialRecordSize);
  record[7] = static_cast<uint8_t>(kCredentialRecordSize >> 8);
  record[8] = static_cast<uint8_t>(ssid.length());
  record[9] = static_cast<uint8_t>(password.length());
  memcpy(record + kCredentialSSIDOffset, ssid.c_str(), ssid.length());
  memcpy(record + kCredentialPasswordOffset, password.c_str(),
         password.length());

  const uint32_t crc = credentialCRC32(record, kCredentialCRCOffset);
  record[kCredentialCRCOffset] = static_cast<uint8_t>(crc);
  record[kCredentialCRCOffset + 1] = static_cast<uint8_t>(crc >> 8);
  record[kCredentialCRCOffset + 2] = static_cast<uint8_t>(crc >> 16);
  record[kCredentialCRCOffset + 3] = static_cast<uint8_t>(crc >> 24);
  return true;
}

bool ESP32WiFiPortal::deserializeCredentialRecord(const uint8_t* record,
                                                  size_t recordSize,
                                                  String& ssid,
                                                  String& password) {
  if (!record || recordSize != kCredentialRecordSize) return false;

  const uint32_t magic = static_cast<uint32_t>(record[0]) |
                         (static_cast<uint32_t>(record[1]) << 8) |
                         (static_cast<uint32_t>(record[2]) << 16) |
                         (static_cast<uint32_t>(record[3]) << 24);
  const uint16_t version = static_cast<uint16_t>(record[4]) |
                           (static_cast<uint16_t>(record[5]) << 8);
  const uint16_t encodedSize = static_cast<uint16_t>(record[6]) |
                               (static_cast<uint16_t>(record[7]) << 8);
  const uint32_t storedCRC =
      static_cast<uint32_t>(record[kCredentialCRCOffset]) |
      (static_cast<uint32_t>(record[kCredentialCRCOffset + 1]) << 8) |
      (static_cast<uint32_t>(record[kCredentialCRCOffset + 2]) << 16) |
      (static_cast<uint32_t>(record[kCredentialCRCOffset + 3]) << 24);
  if (magic != kCredentialMagic || version != kCredentialVersion ||
      encodedSize != kCredentialRecordSize ||
      storedCRC != credentialCRC32(record, kCredentialCRCOffset)) {
    return false;
  }

  const size_t ssidLength = record[8];
  const size_t passwordLength = record[9];
  if (ssidLength == 0 || ssidLength > 32 || passwordLength > 64 ||
      (passwordLength > 0 && passwordLength < 8)) {
    return false;
  }

  for (size_t i = 0; i < kCredentialSSIDCapacity; ++i) {
    if ((i < ssidLength && record[kCredentialSSIDOffset + i] == 0) ||
        (i >= ssidLength && record[kCredentialSSIDOffset + i] != 0)) {
      return false;
    }
  }
  for (size_t i = 0; i < kCredentialPasswordCapacity; ++i) {
    if ((i < passwordLength && record[kCredentialPasswordOffset + i] == 0) ||
        (i >= passwordLength &&
         record[kCredentialPasswordOffset + i] != 0)) {
      return false;
    }
  }

  char ssidBuffer[kCredentialSSIDCapacity];
  char passwordBuffer[kCredentialPasswordCapacity];
  memset(ssidBuffer, 0, sizeof(ssidBuffer));
  memset(passwordBuffer, 0, sizeof(passwordBuffer));
  memcpy(ssidBuffer, record + kCredentialSSIDOffset, ssidLength);
  memcpy(passwordBuffer, record + kCredentialPasswordOffset, passwordLength);
  ssid = ssidBuffer;
  password = passwordBuffer;
  secureClear(ssidBuffer, sizeof(ssidBuffer));
  secureClear(passwordBuffer, sizeof(passwordBuffer));
  return validSTACredentials(ssid, password);
}

void ESP32WiFiPortal::secureClear(void* data, size_t length) {
  volatile uint8_t* bytes = static_cast<volatile uint8_t*>(data);
  while (length-- > 0) *bytes++ = 0;
}

ESP32WiFiPortal::CredentialCacheStatus ESP32WiFiPortal::readCredentialBlob(
    Preferences& prefs,
    const char* key,
    String& ssid,
    String& password) {
  if (!prefs.isKey(key)) {
    return CredentialCacheStatus::NotFound;
  }
  if (prefs.getBytesLength(key) != kCredentialRecordSize) {
    return CredentialCacheStatus::Corrupt;
  }

  uint8_t record[kCredentialRecordSize];
  const size_t bytesRead = prefs.getBytes(key, record, sizeof(record));
  const bool valid = bytesRead == sizeof(record) &&
                     deserializeCredentialRecord(record, sizeof(record),
                                                 ssid, password);
  secureClear(record, sizeof(record));
  return valid ? CredentialCacheStatus::Valid
               : CredentialCacheStatus::Corrupt;
}

ESP32WiFiPortal::CredentialCacheStatus ESP32WiFiPortal::readLegacyCredentials(
    Preferences& prefs,
    String& ssid,
    String& password) {
  const bool hasSSID = prefs.isKey(kPrefsSSID);
  const bool hasPassword = prefs.isKey(kPrefsPassword);
  if (!hasSSID && !hasPassword) return CredentialCacheStatus::NotFound;
  if (!hasSSID || !hasPassword) return CredentialCacheStatus::Corrupt;

  ssid = prefs.getString(kPrefsSSID, "");
  password = prefs.getString(kPrefsPassword, "");
  return validSTACredentials(ssid, password)
             ? CredentialCacheStatus::Valid
             : CredentialCacheStatus::Corrupt;
}

bool ESP32WiFiPortal::writeCredentialRecord(Preferences& prefs,
                                            const String& ssid,
                                            const String& password) {
  uint8_t record[kCredentialRecordSize];
  if (!serializeCredentialRecord(ssid, password, record, sizeof(record))) {
    secureClear(record, sizeof(record));
    return false;
  }

  String currentSSID;
  String currentPassword;
  const CredentialCacheStatus currentStatus = readCredentialBlob(
      prefs, kPrefsCredential, currentSSID, currentPassword);
  if (currentStatus == CredentialCacheStatus::Valid &&
      currentSSID == ssid && currentPassword == password) {
    if (prefs.isKey(kPrefsCredentialBackup)) {
      prefs.remove(kPrefsCredentialBackup);
    }
    secureClear(record, sizeof(record));
    return true;
  }

  bool backupReady = false;
  if (currentStatus == CredentialCacheStatus::Valid) {
    uint8_t oldRecord[kCredentialRecordSize];
    const bool serialized = serializeCredentialRecord(
        currentSSID, currentPassword, oldRecord, sizeof(oldRecord));
    backupReady = serialized && writeCredentialBytes(
        prefs, kPrefsCredentialBackup, oldRecord,
        currentSSID, currentPassword);
    secureClear(oldRecord, sizeof(oldRecord));
    if (!backupReady) {
      secureClear(record, sizeof(record));
      return false;
    }
  } else {
    String backupSSID;
    String backupPassword;
    backupReady = readCredentialBlob(prefs, kPrefsCredentialBackup,
                                     backupSSID, backupPassword) ==
                  CredentialCacheStatus::Valid;

    // If the primary became unreadable during an earlier failed update, the
    // synchronized RAM cache is another valid source for the old value.
    if (!backupReady && _credentialCacheStatus == CredentialCacheStatus::Valid &&
        validSTACredentials(_savedSSID, _savedPassword)) {
      uint8_t oldRecord[kCredentialRecordSize];
      const bool serialized = serializeCredentialRecord(
          _savedSSID, _savedPassword, oldRecord, sizeof(oldRecord));
      backupReady = serialized && writeCredentialBytes(
          prefs, kPrefsCredentialBackup, oldRecord,
          _savedSSID, _savedPassword);
      secureClear(oldRecord, sizeof(oldRecord));
      if (!backupReady) {
        secureClear(record, sizeof(record));
        return false;
      }
    }
  }

  const bool valid = writeCredentialBytes(
      prefs, kPrefsCredential, record, ssid, password);
  secureClear(record, sizeof(record));
  if (valid && prefs.isKey(kPrefsCredentialBackup)) {
    // Failure here is harmless: on boot a valid primary always wins and the
    // stale backup is removed opportunistically.
    prefs.remove(kPrefsCredentialBackup);
  }
  return valid;
}

bool ESP32WiFiPortal::writeCredentialBytes(
    Preferences& prefs,
    const char* key,
    const uint8_t* record,
    const String& expectedSSID,
    const String& expectedPassword) {
  if (!record || prefs.putBytes(key, record, kCredentialRecordSize) !=
                     kCredentialRecordSize ||
      prefs.getBytesLength(key) != kCredentialRecordSize) {
    return false;
  }

  String verifiedSSID;
  String verifiedPassword;
  return readCredentialBlob(prefs, key, verifiedSSID, verifiedPassword) ==
             CredentialCacheStatus::Valid &&
         verifiedSSID == expectedSSID &&
         verifiedPassword == expectedPassword;
}

bool ESP32WiFiPortal::writeCredentialEraseMarker(Preferences& prefs) {
  static const uint8_t marker[] = {'E', 'W', 'P', 'X'};
  uint8_t readBack[sizeof(marker)] = {};
  if (prefs.putBytes(kPrefsCredentialEraseMarker, marker, sizeof(marker)) !=
          sizeof(marker) ||
      prefs.getBytesLength(kPrefsCredentialEraseMarker) != sizeof(marker) ||
      prefs.getBytes(kPrefsCredentialEraseMarker, readBack,
                     sizeof(readBack)) != sizeof(readBack)) {
    return false;
  }
  return memcmp(marker, readBack, sizeof(marker)) == 0;
}

bool ESP32WiFiPortal::removeStoredCredentials(Preferences& prefs) {
  bool removed = true;
  if (prefs.isKey(kPrefsCredential)) {
    removed = prefs.remove(kPrefsCredential) && removed;
  }
  if (prefs.isKey(kPrefsCredentialBackup)) {
    removed = prefs.remove(kPrefsCredentialBackup) && removed;
  }
  if (prefs.isKey(kPrefsSSID)) {
    removed = prefs.remove(kPrefsSSID) && removed;
  }
  if (prefs.isKey(kPrefsPassword)) {
    removed = prefs.remove(kPrefsPassword) && removed;
  }
  return removed;
}

void ESP32WiFiPortal::clearCredentialCache(CredentialCacheStatus status) {
  _savedSSID.remove(0);
  _savedPassword.remove(0);
  _credentialCacheStatus = status;
}

bool ESP32WiFiPortal::hasSavedCredentials() {
  return ensureCredentialCache();
}

String ESP32WiFiPortal::savedSSID() {
  return ensureCredentialCache() ? _savedSSID : String();
}

bool ESP32WiFiPortal::eraseCredentials(bool disconnect) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    setError("Unable to open NVS namespace");
    return false;
  }
  // Commit the erase intent before deleting either transactional record. If
  // power is lost between removes, boot sees the marker and refuses to restore
  // the surviving backup/legacy value.
  const bool markerWritten = writeCredentialEraseMarker(prefs);
  bool ok = false;
  if (markerWritten) {
    const bool removed = removeStoredCredentials(prefs);
    ok = removed && prefs.remove(kPrefsCredentialEraseMarker);
  }
  prefs.end();

  if (ok) {
    clearCredentialCache(CredentialCacheStatus::NotFound);
  } else {
    clearCredentialCache(CredentialCacheStatus::Unknown);
    setError(markerWritten
                 ? "Credential erase is incomplete; the erase marker prevents recovery"
                 : "Unable to verify the credential erase marker");
  }

  if (disconnect) {
    if (!ensureWiFiEventHandler()) {
      if (ok) {
        setError("Credentials erased, but Wi-Fi disconnect requires ownership");
      }
      return false;
    }
    cancelAutoReconnect(true);
    stopConfigPortal();
    const bool wasConnected = WiFi.status() == WL_CONNECTED;
    const bool disconnected = WiFi.disconnect(true, true);
    const bool connectedAfterDisconnect = WiFi.status() == WL_CONNECTED;
    if (!disconnected && wasConnected && connectedAfterDisconnect) {
      setError("Credentials erased, but the STA interface did not disconnect");
      _state = State::Failed;
      ok = false;
    } else {
      _state = connectedAfterDisconnect ? State::Connected : State::Idle;
    }
  }
  return ok;
}

bool ESP32WiFiPortal::setPortalIP(const IPAddress& localIP) {
  return setPortalIP(localIP, localIP, IPAddress(255, 255, 255, 0));
}

bool ESP32WiFiPortal::setPortalIP(const IPAddress& localIP,
                                  const IPAddress& gateway,
                                  const IPAddress& subnet) {
  if (_portalActive) {
    setError("Portal IP cannot be changed while the portal is active");
    return false;
  }
  const PortalNetworkValidationResult validation = validatePortalNetwork(
      ipv4ToUint32(localIP), ipv4ToUint32(gateway), ipv4ToUint32(subnet));
  if (validation != PortalNetworkValidationResult::Valid) {
    setError(portalNetworkValidationMessage(validation));
    return false;
  }

  _portalIP = localIP;
  _portalGateway = gateway;
  _portalSubnet = subnet;
  _lastError = "";
  return true;
}

bool ESP32WiFiPortal::setSTAStaticIP(const IPAddress& localIP,
                                     const IPAddress& gateway,
                                     const IPAddress& subnet,
                                     const IPAddress& primaryDNS,
                                     const IPAddress& secondaryDNS) {
  if (_connectPending || _connectAttemptActive) {
    setError("STA IP cannot be changed during a connection attempt");
    return false;
  }
  const uint32_t local = ipv4ToUint32(localIP);
  const uint32_t gatewayValue = ipv4ToUint32(gateway);
  const uint32_t primaryDNSValue = ipv4ToUint32(primaryDNS);
  const uint32_t secondaryDNSValue = ipv4ToUint32(secondaryDNS);
  if (!isValidSTANetwork(local, gatewayValue, ipv4ToUint32(subnet)) ||
      local == gatewayValue || !isValidDNSAddress(primaryDNSValue) ||
      !isValidDNSAddress(secondaryDNSValue) ||
      (primaryDNSValue == 0 && secondaryDNSValue != 0)) {
    setError("STA IP, gateway, subnet, or DNS configuration is invalid");
    return false;
  }

  _staIP = localIP;
  _staGateway = gateway;
  _staSubnet = subnet;
  _staPrimaryDNS = primaryDNS;
  _staSecondaryDNS = secondaryDNS;
  _staStaticIPEnabled = true;
  _lastError = "";
  return true;
}

void ESP32WiFiPortal::useSTADHCP() {
  _staStaticIPEnabled = false;
  _staIP = IPAddress();
  _staGateway = IPAddress();
  _staSubnet = IPAddress();
  _staPrimaryDNS = IPAddress();
  _staSecondaryDNS = IPAddress();
  _lastError = "";
}

bool ESP32WiFiPortal::isSTAStaticIPConfigured() const {
  return _staStaticIPEnabled;
}

void ESP32WiFiPortal::setAutoReconnect(bool enabled) {
  if (!ensureWiFiEventHandler()) return;
  if (!WiFi.setAutoReconnect(false)) {
    setError("Unable to disable the Arduino Wi-Fi reconnect policy");
    return;
  }
  _autoReconnectEnabled = enabled;
  _lastError = "";

  if (!enabled) {
    const bool hadActiveReconnect =
        _connectionOwner == ConnectionOwner::Reconnect;
    const bool wasReconnecting =
        _reconnectScheduled || hadActiveReconnect;
    cancelAutoReconnect(true);
    if (wasReconnecting) {
      _state = WiFi.status() == WL_CONNECTED ? State::Connected : State::Idle;
    }
    return;
  }

  if (!_portalActive && !_connectAttemptActive &&
      WiFi.status() != WL_CONNECTED) {
    _reconnectRetriesUsed = 0;
    scheduleAutoReconnect(_retryIntervalMs);
  }
}

bool ESP32WiFiPortal::autoReconnectEnabled() const {
  return _autoReconnectEnabled;
}

bool ESP32WiFiPortal::setConnectionRetryPolicy(
    uint8_t retryCount,
    uint32_t retryIntervalMs,
    uint32_t maxRetryIntervalMs) {
  if (retryIntervalMs < 250 || maxRetryIntervalMs < 1000 ||
      maxRetryIntervalMs < retryIntervalMs) {
    setError("Retry interval must be >= 250 ms and maximum >= 1000 ms");
    return false;
  }

  _maxConnectionRetries = retryCount;
  _retryIntervalMs = retryIntervalMs;
  _maxRetryIntervalMs = maxRetryIntervalMs;
  _lastError = "";
  return true;
}

bool ESP32WiFiPortal::setHostname(const char* hostname) {
  if (_wifiEventHandlerId != 0) {
    setError("Hostname must be configured before the first Wi-Fi operation");
    return false;
  }
  if (hostname && strlen(hostname) > 31) {
    setError("Hostname must contain at most 31 bytes");
    return false;
  }

  _hostname = hostname ? hostname : "";
  _lastError = "";
  return true;
}

void ESP32WiFiPortal::setConnectTimeout(uint32_t timeoutMs) {
  if (timeoutMs == 0) {
    _connectTimeoutMs = kDefaultConnectTimeoutMs;
    log(F("[EWP] Connect timeout 0 normalized to 15000 ms"));
    return;
  }
  _connectTimeoutMs = timeoutMs;
}

void ESP32WiFiPortal::setAPChannel(uint8_t channel) {
  if (channel >= 1 && channel <= 13) _apChannel = channel;
}

void ESP32WiFiPortal::setAPHidden(bool hidden) {
  _apHidden = hidden;
}

void ESP32WiFiPortal::setLogging(bool enabled) {
  _loggingEnabled = enabled;
}

void ESP32WiFiPortal::onPortalStarted(Callback callback) {
  _onPortalStarted = std::move(callback);
}

void ESP32WiFiPortal::onCredentialsSaved(Callback callback) {
  _onCredentialsSaved = std::move(callback);
}

void ESP32WiFiPortal::onConnected(Callback callback) {
  _onConnected = std::move(callback);
}

bool ESP32WiFiPortal::isPortalActive() const {
  return _portalActive;
}

bool ESP32WiFiPortal::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

ESP32WiFiPortal::State ESP32WiFiPortal::state() const {
  return _state;
}

IPAddress ESP32WiFiPortal::portalIP() const {
  return _portalIP;
}

String ESP32WiFiPortal::portalSSID() const {
  return _portalSSID;
}

String ESP32WiFiPortal::lastError() const {
  return _lastError;
}

uint8_t ESP32WiFiPortal::lastDisconnectReason() const {
  return _lastDisconnectReason;
}

bool ESP32WiFiPortal::validAPPassword(const char* password) const {
  if (!password || strlen(password) == 0) return true;
  const size_t len = strlen(password);
  return len >= 8 && len <= 63;
}

bool ESP32WiFiPortal::portalTimedOut() const {
  return _portalTimeoutMs > 0 && (millis() - _portalStartedAt >= _portalTimeoutMs);
}

bool ESP32WiFiPortal::isCredentialFailureReason(uint8_t reason) const {
  switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return true;
    default:
      return false;
  }
}

void ESP32WiFiPortal::setError(const String& message) {
  _lastError = message;
}

void ESP32WiFiPortal::invoke(const Callback& callback) {
  if (callback) callback();
}

void ESP32WiFiPortal::log(const __FlashStringHelper* message) const {
  if (_loggingEnabled) Serial.println(message);
}

void ESP32WiFiPortal::logDisconnect(uint8_t reason) const {
  if (!_loggingEnabled) return;
  Serial.print(F("[EWP] Disconnect: "));
  Serial.print(reason);
  const char* reasonName =
      WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason));
  if (reasonName && reasonName[0] != '\0') {
    Serial.print(F(" ("));
    Serial.print(reasonName);
    Serial.print(')');
  }
  Serial.println();
}
