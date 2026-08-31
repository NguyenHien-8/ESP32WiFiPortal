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

bool isValidPortalNetwork(const IPAddress& localIP,
                          const IPAddress& gateway,
                          const IPAddress& subnet) {
  if (!isPrivateIPv4(localIP) || !isPrivateIPv4(gateway)) return false;

  const uint32_t local = ipToUint32(localIP);
  const uint32_t gatewayValue = ipToUint32(gateway);
  const uint32_t mask = ipToUint32(subnet);
  if (mask == 0 || mask == 0xFFFFFFFFUL) return false;

  const uint32_t hostMask = ~mask;
  if ((hostMask & (hostMask + 1UL)) != 0) return false;

  uint32_t minimumPrivateMask = 0xFF000000UL;  // 10.0.0.0/8
  if (localIP[0] == 172) minimumPrivateMask = 0xFFF00000UL;  // 172.16.0.0/12
  if (localIP[0] == 192) minimumPrivateMask = 0xFFFF0000UL;  // 192.168.0.0/16
  if ((mask & minimumPrivateMask) != minimumPrivateMask) return false;

  if ((local & mask) != (gatewayValue & mask)) return false;

  const uint32_t localHost = local & hostMask;
  const uint32_t gatewayHost = gatewayValue & hostMask;
  return localHost != 0 && localHost != hostMask &&
         gatewayHost != 0 && gatewayHost != hostMask;
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
  stopConfigPortal();
}

bool ESP32WiFiPortal::connectSaved(uint32_t timeoutMs) {
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

  stopConfigPortal();
  _lastError = "";
  _portalSSID = apSSID;
  _portalTimeoutMs = portalTimeoutMs;
  _portalStartedAt = millis();
  _connectPending = false;
  _connectAttemptActive = false;

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
  _lastError = "";

  _server->send_P(200, "text/html; charset=utf-8", EWP_CONNECTING_HTML);
}

void ESP32WiFiPortal::handleStatus() {
  if (!_server) return;
  String json = F("{\"connected\":");
  json += isConnected() ? F("true") : F("false");
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
  if (!_portalActive) return;

  _dns.processNextRequest();
  if (_server) _server->handleClient();

  if (portalTimedOut()) {
    const bool hadCandidateAttempt = _connectAttemptActive;
    setError("Configuration portal timed out");
    stopConfigPortal();
    _state = !hadCandidateAttempt && WiFi.status() == WL_CONNECTED
                 ? State::Connected
                 : State::Failed;
    return;
  }

  if (_connectPending && !_connectAttemptActive && millis() - _connectPendingAt >= 350) {
    beginPendingConnection();
  }

  if (_connectAttemptActive) {
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

    if (_connectTimeoutMs > 0 && millis() - _connectAttemptAt >= _connectTimeoutMs) {
      clearPendingConnection(true);
      _state = State::Portal;
      setError("Unable to connect. Check the SSID/password and try again.");
    }
  }
}

void ESP32WiFiPortal::beginPendingConnection() {
  if (!_connectPending || _connectAttemptActive) return;

  _connectPending = false;
  _connectAttemptActive = true;
  _connectAttemptAt = millis();
  _lastError = "";

  // Keep AP + captive portal alive while testing the STA credentials.
  WiFi.mode(WIFI_AP_STA);
  if (_hostname.length() > 0) {
    WiFi.setHostname(_hostname.c_str());
    WiFi.softAPsetHostname(_hostname.c_str());
  }
  WiFi.disconnect(false, false);
  delay(20);
  WiFi.begin(_pendingSSID.c_str(), _pendingPassword.c_str());
}

void ESP32WiFiPortal::clearPendingConnection(bool disconnectSTA) {
  const bool shouldDisconnect = disconnectSTA && _connectAttemptActive;
  _connectPending = false;
  _connectAttemptActive = false;
  _connectPendingAt = 0;
  _connectAttemptAt = 0;
  _pendingSSID = String();
  _pendingPassword = String();

  if (shouldDisconnect) {
    WiFi.disconnect(false, false);
  }
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

  _state = State::Connecting;
  _lastError = "";
  WiFi.mode(WIFI_STA);
  if (_hostname.length() > 0) {
    WiFi.setHostname(_hostname.c_str());
  }
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (timeoutMs > 0 && millis() - started >= timeoutMs) {
      WiFi.disconnect(false, false);
      setError("Wi-Fi connection timed out");
      _state = State::Failed;
      return false;
    }
    delay(50);
    yield();
  }

  _state = State::Connected;
  invoke(_onConnected);
  return true;
}

void ESP32WiFiPortal::stopConfigPortal() {
  const bool wasPortalActive = _portalActive;
  const bool hadCandidateAttempt = _connectAttemptActive;
  _portalActive = false;
  clearPendingConnection(true);

  if (_server) {
    _server->stop();
    _server.reset();
  }
  _dns.stop();

  if (wasPortalActive) {
    WiFi.softAPdisconnect(true);
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

bool ESP32WiFiPortal::validAPPassword(const char* password) const {
  if (!password || strlen(password) == 0) return true;
  const size_t len = strlen(password);
  return len >= 8 && len <= 63;
}

bool ESP32WiFiPortal::portalTimedOut() const {
  return _portalTimeoutMs > 0 && (millis() - _portalStartedAt >= _portalTimeoutMs);
}

void ESP32WiFiPortal::setError(const String& message) {
  _lastError = message;
}

void ESP32WiFiPortal::invoke(const Callback& callback) {
  if (callback) callback();
}
