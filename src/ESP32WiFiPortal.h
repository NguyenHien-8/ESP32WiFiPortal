#pragma once

#include <Arduino.h>

#if !defined(ESP32)
#error "ESP32WiFiPortal supports ESP32 Arduino Core only."
#endif

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <functional>
#include <memory>
#include <utility>

class ESP32WiFiPortal {
public:
  enum class State : uint8_t {
    Idle,
    Connecting,
    Connected,
    Portal,
    Failed
  };

  using Callback = std::function<void()>;

  ESP32WiFiPortal();
  ~ESP32WiFiPortal();

  ESP32WiFiPortal(const ESP32WiFiPortal&) = delete;
  ESP32WiFiPortal& operator=(const ESP32WiFiPortal&) = delete;

  // Connect using credentials stored by this library in ESP32 NVS.
  bool connectSaved(uint32_t timeoutMs = 15000);

  // Convenience startup: try saved Wi-Fi, then optionally open a blocking portal.
  bool autoConnect(const char* apSSID = "ESP32-Setup",
                   const char* apPassword = nullptr,
                   uint32_t connectTimeoutMs = 15000,
                   uint32_t portalTimeoutMs = 0);

  // Blocking captive portal. Returns true after successful Wi-Fi connection.
  // portalTimeoutMs == 0 means no portal timeout.
  bool startConfigPortal(const char* apSSID = "ESP32-Setup",
                         const char* apPassword = nullptr,
                         uint32_t portalTimeoutMs = 0);

  // Non-blocking captive portal. Call process() frequently from loop().
  bool startConfigPortalAsync(const char* apSSID = "ESP32-Setup",
                              const char* apPassword = nullptr,
                              uint32_t portalTimeoutMs = 0);

  void process();
  void stopConfigPortal();

  bool isPortalActive() const;
  bool isConnected() const;
  State state() const;

  // Credential management.
  bool hasSavedCredentials();
  String savedSSID();
  bool eraseCredentials(bool disconnect = true);

  // Optional tuning.
  void setHostname(const char* hostname);
  void setConnectTimeout(uint32_t timeoutMs);
  void setAPChannel(uint8_t channel);
  void setAPHidden(bool hidden);

  // Event callbacks.
  void onPortalStarted(Callback callback);
  void onCredentialsSaved(Callback callback);
  void onConnected(Callback callback);

  IPAddress portalIP() const;
  String portalSSID() const;
  String lastError() const;

private:
  static constexpr uint16_t kDnsPort = 53;
  static constexpr uint16_t kHttpPort = 80;
  static constexpr const char* kPrefsNamespace = "ewp_wifi";
  static constexpr const char* kPrefsSSID = "ssid";
  static constexpr const char* kPrefsPassword = "pass";

  bool openPortal(const char* apSSID, const char* apPassword, uint32_t portalTimeoutMs);
  void configureRoutes();
  void handleRoot();
  void handleScan();
  void handleSave();
  void handleStatus();
  void handleNotFound();
  void handleCaptiveProbe();
  void beginPendingConnection();

  bool connect(const String& ssid, const String& password, uint32_t timeoutMs);
  bool saveCredentials(const String& ssid, const String& password);
  bool loadCredentials(String& ssid, String& password);
  bool validAPPassword(const char* password) const;
  bool portalTimedOut() const;
  void setError(const String& message);
  void invoke(const Callback& callback);

  std::unique_ptr<WebServer> _server;
  DNSServer _dns;

  State _state = State::Idle;
  bool _portalActive = false;
  bool _blockingPortal = false;
  bool _connectPending = false;
  bool _connectAttemptActive = false;
  bool _routesConfigured = false;

  uint32_t _connectTimeoutMs = 15000;
  uint32_t _portalTimeoutMs = 0;
  uint32_t _portalStartedAt = 0;
  uint32_t _connectPendingAt = 0;
  uint32_t _connectAttemptAt = 0;

  uint8_t _apChannel = 1;
  bool _apHidden = false;

  String _hostname;
  String _portalSSID;
  String _pendingSSID;
  String _pendingPassword;
  String _lastError;

  Callback _onPortalStarted;
  Callback _onCredentialsSaved;
  Callback _onConnected;
};
