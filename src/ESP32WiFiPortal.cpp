/**
 * @file ESP32WiFiPortal.cpp
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief ESP32 Wi-Fi captive portal library implementation
 * @version 1.1.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include "ESP32WiFiPortal.h"
#include "PortalPage.h"

namespace {
uint32_t ipToUint32(const IPAddress& address) {
  return (static_cast<uint32_t>(address[0]) << 24) |
         (static_cast<uint32_t>(address[1]) << 16) |
         (static_cast<uint32_t>(address[2]) << 8) |
         static_cast<uint32_t>(address[3]);
}

bool isPrivateIPv4(const IPAddress& address) {
  return address[0] == 10 ||
         (address[0] == 172 && address[1] >= 16 && address[1] <= 31) ||
         (address[0] == 192 && address[1] == 168);
}

bool isUsableUnicastIPv4(const IPAddress& address) {
  const uint32_t value = ipToUint32(address);
  return value != 0 && value != 0xFFFFFFFFUL && address[0] != 0 &&
         address[0] != 127 && address[0] < 224;
}

bool isValidIPv4Network(const IPAddress& localIP,
                        const IPAddress& gateway,
                        const IPAddress& subnet) {
  if (!isUsableUnicastIPv4(localIP) || !isUsableUnicastIPv4(gateway)) {
    return false;
  }

  const uint32_t local = ipToUint32(localIP);
  const uint32_t gatewayValue = ipToUint32(gateway);
  const uint32_t mask = ipToUint32(subnet);
  if (mask == 0 || mask == 0xFFFFFFFFUL) return false;

  const uint32_t hostMask = ~mask;
  if ((hostMask & (hostMask + 1UL)) != 0) return false;
  if ((local & mask) != (gatewayValue & mask)) return false;

  const uint32_t localHost = local & hostMask;
  const uint32_t gatewayHost = gatewayValue & hostMask;
  return localHost != 0 && localHost != hostMask &&
         gatewayHost != 0 && gatewayHost != hostMask;
}

bool isValidPortalNetwork(const IPAddress& localIP,
                          const IPAddress& gateway,
                          const IPAddress& subnet) {
  if (!isPrivateIPv4(localIP) || !isPrivateIPv4(gateway) ||
      !isValidIPv4Network(localIP, gateway, subnet)) {
    return false;
  }

  const uint32_t mask = ipToUint32(subnet);
  uint32_t minimumPrivateMask = 0xFF000000UL;  // 10.0.0.0/8
  if (localIP[0] == 172) minimumPrivateMask = 0xFFF00000UL;  // 172.16.0.0/12
  if (localIP[0] == 192) minimumPrivateMask = 0xFFFF0000UL;  // 192.168.0.0/16
  return (mask & minimumPrivateMask) == minimumPrivateMask;
}

bool isValidDNS(const IPAddress& address) {
  return ipToUint32(address) == 0 || isUsableUnicastIPv4(address);
}
}  // namespace

constexpr uint16_t ESP32WiFiPortal::kDnsPort;
constexpr uint16_t ESP32WiFiPortal::kHttpPort;
constexpr const char* ESP32WiFiPortal::kPrefsNamespace;
constexpr const char* ESP32WiFiPortal::kPrefsSSID;
constexpr const char* ESP32WiFiPortal::kPrefsPassword;

ESP32WiFiPortal::ESP32WiFiPortal()
    : _portalIP(192, 168, 4, 1),
      _portalGateway(192, 168, 4, 1),
      _portalSubnet(255, 255, 255, 0) {}

ESP32WiFiPortal::~ESP32WiFiPortal() {
  const bool restoreCoreAutoReconnect = _wifiEventHandlerId != 0;
  if (_wifiEventHandlerId != 0) {
    WiFi.removeEvent(_wifiEventHandlerId);
    _wifiEventHandlerId = 0;
  }
  stopConfigPortal();
  cancelAutoReconnect(true);
  if (restoreCoreAutoReconnect) {
    WiFi.setAutoReconnect(_coreAutoReconnectWasEnabled);
  }
}

void ESP32WiFiPortal::ensureWiFiEventHandler() {
  if (_wifiEventHandlerId != 0) return;

  _coreAutoReconnectWasEnabled = WiFi.getAutoReconnect();
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
            _wifiEventBits.fetch_or(kEventSTADisconnected);
            break;
          default:
            break;
        }
      });

  if (_wifiEventHandlerId != 0) {
    // The library owns reconnect timing. Leaving the core policy enabled would
    // create a second, uncoordinated reconnect path from the event task.
    WiFi.setAutoReconnect(false);
  }
}

bool ESP32WiFiPortal::connectSaved(uint32_t timeoutMs) {
  ensureWiFiEventHandler();
  cancelAutoReconnect(true);
  if (_portalActive) {
    stopConfigPortal();
  }

  String ssid;
  String password;
  if (!loadCredentials(ssid, password) || ssid.length() == 0) {
    setError("No saved Wi-Fi credentials");
    _state = State::Failed;
    return false;
  }
  return connect(ssid, password, timeoutMs);
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
  if (!apSSID || strlen(apSSID) == 0) {
    setError("AP SSID cannot be empty");
    return false;
  }
  if (!validAPPassword(apPassword)) {
    setError("AP password must be empty or 8-63 characters");
    return false;
  }

  ensureWiFiEventHandler();
  cancelAutoReconnect(true);
  stopConfigPortal();
  _lastError = "";
  _portalSSID = apSSID;
  _portalTimeoutMs = portalTimeoutMs;
  _portalStartedAt = millis();
  _connectPending = false;
  _connectAttemptActive = false;
  _connectionOwner = ConnectionOwner::None;
  _portalRetriesUsed = 0;

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAPConfig(_portalIP, _portalGateway, _portalSubnet)) {
    WiFi.softAPdisconnect(true);
    setError("Failed to configure captive portal IP");
    _state = State::Failed;
    return false;
  }

  if (_hostname.length() > 0) {
    WiFi.setHostname(_hostname.c_str());
    WiFi.softAPsetHostname(_hostname.c_str());
  }

  bool apOk = false;
  if (apPassword && strlen(apPassword) > 0) {
    apOk = WiFi.softAP(apSSID, apPassword, _apChannel, _apHidden ? 1 : 0);
  } else {
    apOk = WiFi.softAP(apSSID, nullptr, _apChannel, _apHidden ? 1 : 0);
  }

  if (!apOk) {
    WiFi.softAPdisconnect(true);
    setError("Failed to start ESP32 access point");
    _state = State::Failed;
    return false;
  }

  _portalActive = true;
  _server.reset(new WebServer(kHttpPort));
  configureRoutes();
  _server->begin();

  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (!_dns.start(kDnsPort, "*", WiFi.softAPIP())) {
    setError("Failed to start captive portal DNS");
    stopConfigPortal();
    _state = State::Failed;
    return false;
  }

  _state = State::Portal;
  if (_loggingEnabled) {
    Serial.print(F("[EWP] Portal started: http://"));
    Serial.println(_portalIP);
  }
  invoke(_onPortalStarted);
  return true;
}

void ESP32WiFiPortal::configureRoutes() {
  if (!_server) return;

  _server->on("/", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/wifi", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/scan", HTTP_GET, [this]() { handleScan(); });
  _server->on("/save", HTTP_POST, [this]() { handleSave(); });
  _server->on("/status", HTTP_GET, [this]() { handleStatus(); });

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

void ESP32WiFiPortal::handleScan() {
  if (!_server) return;

  int count = WiFi.scanNetworks(false, true);
  if (count < 0) count = 0;

  String json;
  json.reserve(96 + static_cast<size_t>(count) * 80);
  json = F("{\"networks\":[");

  // Emit unique SSIDs only, strongest first (Arduino scan is normally RSSI sorted).
  for (int i = 0; i < count; ++i) {
    const String currentSSID = WiFi.SSID(i);
    if (currentSSID.length() == 0) continue;

    bool duplicate = false;
    for (int j = 0; j < i; ++j) {
      if (WiFi.SSID(j) == currentSSID) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    if (json.length() > strlen("{\"networks\":[")) json += ',';

    String escaped = currentSSID;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");

    const bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    json += F("{\"ssid\":\"");
    json += escaped;
    json += F("\",\"rssi\":");
    json += WiFi.RSSI(i);
    json += F(",\"open\":");
    json += open ? F("true") : F("false");
    json += '}';
  }

  json += F("]}");
  WiFi.scanDelete();

  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "application/json; charset=utf-8", json);
}

void ESP32WiFiPortal::handleSave() {
  if (!_server) return;

  String ssid = _server->arg("ssid");
  String password = _server->arg("password");
  ssid.trim();

  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
    _server->send(400, "text/plain; charset=utf-8", "Invalid SSID or password length");
    return;
  }

  if (_connectPending || _connectAttemptActive) {
    _server->send(409, "text/plain; charset=utf-8", "A Wi-Fi connection attempt is already running");
    return;
  }

  _pendingSSID = ssid;
  _pendingPassword = password;
  _connectPending = true;
  _connectAttemptActive = false;
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
  String json = F("{\"connected\":");
  json += isConnected() && !candidateInProgress ? F("true") : F("false");
  json += F(",\"portal\":");
  json += isPortalActive() ? F("true") : F("false");
  json += F(",\"ip\":\"");
  json += isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  json += F("\",\"error\":\"");
  String escapedError = _lastError;
  escapedError.replace("\\", "\\\\");
  escapedError.replace("\"", "\\\"");
  json += escapedError;
  json += F("\"}");
  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "application/json; charset=utf-8", json);
}

void ESP32WiFiPortal::handleCaptiveProbe() {
  if (!_server) return;
  _server->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  _server->send(302, "text/plain", "");
}

void ESP32WiFiPortal::handleNotFound() {
  if (!_server) return;
  handleCaptiveProbe();
}

void ESP32WiFiPortal::process() {
  processWiFiEvents();

  if (_portalActive) {
    _dns.processNextRequest();
    if (_server) _server->handleClient();

    if (portalTimedOut()) {
      const bool hadCandidateAttempt =
          _connectAttemptActive && _connectionOwner == ConnectionOwner::Portal;
      setError("Configuration portal timed out");
      log(F("[EWP] Portal timeout"));
      stopConfigPortal();
      _state = !hadCandidateAttempt && WiFi.status() == WL_CONNECTED
                   ? State::Connected
                   : State::Failed;
      return;
    }

    if (_connectPending && !_connectAttemptActive &&
        millis() - _connectPendingAt >= _connectPendingDelayMs) {
      beginPendingConnection();
    }

    if (_connectAttemptActive && _connectionOwner == ConnectionOwner::Portal) {
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
      } else if (_connectTimeoutMs > 0 &&
                 millis() - _connectAttemptAt >= _connectTimeoutMs) {
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
  if (!beginSTAConnection(_pendingSSID, _pendingPassword,
                          ConnectionOwner::Portal)) {
    clearPendingConnection(false);
    _state = State::Portal;
  }
}

void ESP32WiFiPortal::clearPendingConnection(bool disconnectSTA) {
  const bool ownsSTA = _connectionOwner == ConnectionOwner::Portal;
  if (disconnectSTA && ownsSTA && _connectAttemptActive) {
    cancelSTAConnection();
  } else if (ownsSTA) {
    _connectAttemptActive = false;
    _connectionOwner = ConnectionOwner::None;
    _connectAttemptAt = 0;
    _attemptTerminalFailure = false;
  }

  _connectPending = false;
  _connectPendingAt = 0;
  _connectPendingDelayMs = 350;
  _portalRetriesUsed = 0;
  _pendingSSID = String();
  _pendingPassword = String();
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

bool ESP32WiFiPortal::beginSTAConnection(const String& ssid,
                                         const String& password,
                                         ConnectionOwner owner) {
  ensureWiFiEventHandler();
  WiFi.setAutoReconnect(false);

  processWiFiEvents();
  WiFi.mode(_portalActive ? WIFI_AP_STA : WIFI_STA);
  if (_hostname.length() > 0) {
    WiFi.setHostname(_hostname.c_str());
    if (_portalActive) WiFi.softAPsetHostname(_hostname.c_str());
  }

  WiFi.disconnect(false, false);
  delay(20);
  if (!applySTAConfig()) {
    setError("Failed to apply STA IP/DNS configuration");
    return false;
  }

  _connectionOwner = owner;
  _connectAttemptActive = true;
  _connectAttemptAt = millis();
  _attemptTerminalFailure = false;
  log(F("[EWP] Connect"));

  const wl_status_t result = WiFi.begin(ssid.c_str(), password.c_str());
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

void ESP32WiFiPortal::cancelSTAConnection() {
  const bool wasActive = _connectAttemptActive;
  _connectAttemptActive = false;
  _connectionOwner = ConnectionOwner::None;
  _connectAttemptAt = 0;
  _attemptTerminalFailure = false;
  if (wasActive) WiFi.disconnect(false, false);
}

void ESP32WiFiPortal::processWiFiEvents() {
  const uint32_t events = _wifiEventBits.exchange(0);
  if (events == 0) return;

  const bool connectedNow = WiFi.status() == WL_CONNECTED;
  if ((events & kEventSTAConnected) != 0 && _loggingEnabled) {
    Serial.println(F("[EWP] STA connected"));
  }

  if ((events & kEventSTAGotIP) != 0 && connectedNow) {
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
  if (reason == 0) reason = WIFI_REASON_UNSPECIFIED;
  _lastDisconnectReason = reason;
  logDisconnect(reason);
  if (connectedNow || reason == WIFI_REASON_ASSOC_LEAVE) return;

  if (_connectAttemptActive) {
    _attemptTerminalFailure =
        _attemptTerminalFailure || isTerminalDisconnectReason(reason);
    return;
  }

  if (_autoReconnectEnabled && !_portalActive && _state == State::Connected) {
    if (isTerminalDisconnectReason(reason)) {
      setError("Auto reconnect stopped after an authentication failure");
      _state = State::Failed;
    } else {
      _reconnectRetriesUsed = 0;
      scheduleAutoReconnect(_retryIntervalMs);
    }
  }
}

void ESP32WiFiPortal::processAutoReconnect() {
  if (!_autoReconnectEnabled || _portalActive) return;

  if (_connectAttemptActive && _connectionOwner == ConnectionOwner::Reconnect) {
    if (WiFi.status() == WL_CONNECTED) {
      _connectAttemptActive = false;
      _connectionOwner = ConnectionOwner::None;
      _connectAttemptAt = 0;
      _attemptTerminalFailure = false;
      _reconnectScheduled = false;
      _reconnectRetriesUsed = 0;
      _lastError = "";
      _state = State::Connected;
      invoke(_onConnected);
      return;
    }

    if (_attemptTerminalFailure) {
      cancelSTAConnection();
      _reconnectScheduled = false;
      setError("Auto reconnect stopped after an authentication failure");
      _state = State::Failed;
      log(F("[EWP] Reconnect stopped: authentication failure"));
      return;
    }

    if (_connectTimeoutMs == 0 ||
        millis() - _connectAttemptAt < _connectTimeoutMs) {
      return;
    }

    cancelSTAConnection();
    if (_reconnectRetriesUsed < _maxConnectionRetries) {
      ++_reconnectRetriesUsed;
      scheduleAutoReconnect(retryDelay(_reconnectRetriesUsed));
    } else {
      _reconnectRetriesUsed = 0;
      scheduleAutoReconnect(_maxRetryIntervalMs);
      log(F("[EWP] Reconnect cooldown"));
    }
    return;
  }

  if (!_reconnectScheduled || _connectAttemptActive ||
      millis() - _reconnectScheduledAt < _reconnectDelayMs) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _reconnectScheduled = false;
    _reconnectRetriesUsed = 0;
    _state = State::Connected;
    return;
  }

  String ssid;
  String password;
  if (!loadCredentials(ssid, password) || ssid.length() == 0) {
    _reconnectScheduled = false;
    setError("Auto reconnect requires saved Wi-Fi credentials");
    _state = State::Failed;
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
  if (!beginSTAConnection(ssid, password, ConnectionOwner::Reconnect)) {
    _state = State::Failed;
  }
}

void ESP32WiFiPortal::scheduleAutoReconnect(uint32_t delayMs) {
  if (!_autoReconnectEnabled || _portalActive) return;
  _reconnectScheduled = true;
  _reconnectScheduledAt = millis();
  _reconnectDelayMs = delayMs;
  _state = State::Connecting;
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
      _connectAttemptActive = false;
      _connectionOwner = ConnectionOwner::None;
      _connectAttemptAt = 0;
      _attemptTerminalFailure = false;
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

bool ESP32WiFiPortal::connect(const String& ssid,
                              const String& password,
                              uint32_t timeoutMs) {
  if (ssid.length() == 0) {
    setError("SSID cannot be empty");
    _state = State::Failed;
    return false;
  }

  if (_portalActive) {
    stopConfigPortal();
  }

  ensureWiFiEventHandler();
  cancelAutoReconnect(true);
  _state = State::Connecting;
  _lastError = "";
  uint8_t retriesUsed = 0;

  while (true) {
    if (!beginSTAConnection(ssid, password, ConnectionOwner::Blocking)) {
      _state = State::Failed;
      return false;
    }

    while (WiFi.status() != WL_CONNECTED) {
      processWiFiEvents();
      if (_attemptTerminalFailure ||
          (timeoutMs > 0 && millis() - _connectAttemptAt >= timeoutMs)) {
        break;
      }
      delay(10);
      yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
      _connectAttemptActive = false;
      _connectionOwner = ConnectionOwner::None;
      _connectAttemptAt = 0;
      _attemptTerminalFailure = false;
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
  const bool hadCandidateAttempt =
      _connectAttemptActive && _connectionOwner == ConnectionOwner::Portal;
  _portalActive = false;
  clearPendingConnection(true);

  if (_server) {
    _server->stop();
    _server.reset();
  }
  _dns.stop();

  if (wasPortalActive) {
    WiFi.softAPdisconnect(true);
    log(F("[EWP] Portal stopped"));
  }

  _portalTimeoutMs = 0;
  _portalStartedAt = 0;
  if (_state == State::Portal) {
    _state = !hadCandidateAttempt && WiFi.status() == WL_CONNECTED
                 ? State::Connected
                 : State::Idle;
  }
}

bool ESP32WiFiPortal::saveCredentials(const String& ssid, const String& password) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    setError("Unable to open NVS namespace");
    return false;
  }
  const size_t ssidWritten = prefs.putString(kPrefsSSID, ssid);
  const size_t passwordWritten = prefs.putString(kPrefsPassword, password);
  prefs.end();
  return ssidWritten == ssid.length() &&
         (password.length() == 0 || passwordWritten == password.length());
}

bool ESP32WiFiPortal::loadCredentials(String& ssid, String& password) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) return false;
  ssid = prefs.getString(kPrefsSSID, "");
  password = prefs.getString(kPrefsPassword, "");
  prefs.end();
  return ssid.length() > 0;
}

bool ESP32WiFiPortal::hasSavedCredentials() {
  String ssid, password;
  return loadCredentials(ssid, password);
}

String ESP32WiFiPortal::savedSSID() {
  String ssid, password;
  loadCredentials(ssid, password);
  return ssid;
}

bool ESP32WiFiPortal::eraseCredentials(bool disconnect) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    setError("Unable to open NVS namespace");
    return false;
  }
  const bool ok = prefs.clear();
  prefs.end();

  if (disconnect) {
    cancelAutoReconnect(true);
    stopConfigPortal();
    WiFi.disconnect(true, true);
    _state = State::Idle;
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
  if (!isValidPortalNetwork(localIP, gateway, subnet)) {
    setError("Portal IP, gateway, or subnet is not a usable private IPv4 network");
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
  if (!isValidIPv4Network(localIP, gateway, subnet) ||
      ipToUint32(localIP) == ipToUint32(gateway) ||
      !isValidDNS(primaryDNS) || !isValidDNS(secondaryDNS) ||
      (ipToUint32(primaryDNS) == 0 && ipToUint32(secondaryDNS) != 0)) {
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
  ensureWiFiEventHandler();
  WiFi.setAutoReconnect(false);
  _autoReconnectEnabled = enabled;

  if (!enabled) {
    const bool hadActiveReconnect =
        _connectionOwner == ConnectionOwner::Reconnect;
    const bool wasReconnecting =
        _reconnectScheduled || hadActiveReconnect;
    cancelAutoReconnect(true);
    if (wasReconnecting) {
      _state = !hadActiveReconnect && WiFi.status() == WL_CONNECTED
                   ? State::Connected
                   : State::Idle;
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

void ESP32WiFiPortal::setHostname(const char* hostname) {
  _hostname = hostname ? hostname : "";
}

void ESP32WiFiPortal::setConnectTimeout(uint32_t timeoutMs) {
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

bool ESP32WiFiPortal::isTerminalDisconnectReason(uint8_t reason) const {
  switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
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
