/**
 * @file ESP32WiFiPortal.h
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief ESP32 Wi-Fi captive portal library header
 * @version 1.1.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#pragma once

#include <Arduino.h>

#if !defined(ESP32)
#error "ESP32WiFiPortal supports ESP32 Arduino Core only."
#endif

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
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
  // The one-argument overload uses localIP as the gateway and a /24 subnet.
  // Portal addressing can only be changed while the portal is stopped.
  bool setPortalIP(const IPAddress& localIP);
  bool setPortalIP(const IPAddress& localIP,
                   const IPAddress& gateway,
                   const IPAddress& subnet);

  // Optional static IPv4 configuration for the STA interface. DHCP remains
  // the default and can be restored with useSTADHCP(). Changes apply to the
  // next library-managed connection attempt.
  bool setSTAStaticIP(const IPAddress& localIP,
                      const IPAddress& gateway,
                      const IPAddress& subnet,
                      const IPAddress& primaryDNS = IPAddress(),
                      const IPAddress& secondaryDNS = IPAddress());
  void useSTADHCP();
  bool isSTAStaticIPConfigured() const;

  // Library-managed reconnect is processed by process(). The retry count is
  // the number of retries after the first attempt. Reconnects enter a capped
  // cooldown after a retry burst, including ambiguous authentication failures.
  void setAutoReconnect(bool enabled);
  bool autoReconnectEnabled() const;
  bool setConnectionRetryPolicy(uint8_t retryCount,
                                uint32_t retryIntervalMs,
                                uint32_t maxRetryIntervalMs);

  void setHostname(const char* hostname);
  void setConnectTimeout(uint32_t timeoutMs);
  void setAPChannel(uint8_t channel);
  void setAPHidden(bool hidden);
  void setLogging(bool enabled);

  // Event callbacks.
  void onPortalStarted(Callback callback);
  void onCredentialsSaved(Callback callback);
  void onConnected(Callback callback);

  IPAddress portalIP() const;
  String portalSSID() const;
  String lastError() const;
  uint8_t lastDisconnectReason() const;

private:
  enum class ConnectionOwner : uint8_t {
    None,
    Blocking,
    Portal,
    Reconnect
  };

  struct ScanNetworkIdentity {
    uint32_t hash;
    int index;
  };

  static constexpr uint16_t kDnsPort = 53;
  static constexpr uint16_t kHttpPort = 80;
  static constexpr const char* kPrefsNamespace = "ewp_wifi";
  static constexpr const char* kPrefsSSID = "ssid";
  static constexpr const char* kPrefsPassword = "pass";
  static constexpr uint32_t kEventSTAConnected = 1UL << 0;
  static constexpr uint32_t kEventSTAGotIP = 1UL << 1;
  static constexpr uint32_t kEventSTADisconnected = 1UL << 2;

  bool openPortal(const char* apSSID, const char* apPassword, uint32_t portalTimeoutMs);
  void configureRoutes();
  void handleRoot();
  void handleScan();
  void handleSave();
  void handleStatus();
  void handleNotFound();
  void handleCaptiveProbe();
  void beginPendingConnection();
  void clearPendingConnection(bool disconnectSTA);
  void failPendingConnection(bool terminalFailure);

  bool connect(const String& ssid, const String& password, uint32_t timeoutMs);
  bool beginSTAConnection(const String& ssid,
                          const String& password,
                          ConnectionOwner owner);
  bool applySTAConfig();
  void cancelSTAConnection();
  void ensureWiFiEventHandler();
  void processWiFiEvents();
  void processAutoReconnect();
  void scheduleAutoReconnect(uint32_t delayMs);
  void scheduleNextAutoReconnect();
  void cancelAutoReconnect(bool disconnectSTA);
  bool scheduleSavedConnectionRecovery();
  uint32_t retryDelay(uint8_t retryNumber) const;
  bool saveCredentials(const String& ssid, const String& password);
  bool ensureCredentialCache();
  bool validAPPassword(const char* password) const;
  bool portalTimedOut() const;
  bool isCredentialFailureReason(uint8_t reason) const;
  void setError(const String& message);
  void invoke(const Callback& callback);
  void log(const __FlashStringHelper* message) const;
  void logDisconnect(uint8_t reason) const;

  std::unique_ptr<WebServer> _server;
  DNSServer _dns;

  State _state = State::Idle;
  ConnectionOwner _connectionOwner = ConnectionOwner::None;
  bool _portalActive = false;
  bool _connectPending = false;
  bool _connectAttemptActive = false;
  bool _attemptTerminalFailure = false;

  uint32_t _connectTimeoutMs = 15000;
  uint32_t _portalTimeoutMs = 0;
  uint32_t _portalStartedAt = 0;
  uint32_t _connectPendingAt = 0;
  uint32_t _connectAttemptAt = 0;
  uint32_t _connectPendingDelayMs = 350;

  uint8_t _maxConnectionRetries = 0;
  uint8_t _portalRetriesUsed = 0;
  uint32_t _retryIntervalMs = 1000;
  uint32_t _maxRetryIntervalMs = 60000;

  bool _autoReconnectEnabled = true;
  bool _reconnectScheduled = false;
  uint8_t _reconnectRetriesUsed = 0;
  uint32_t _reconnectScheduledAt = 0;
  uint32_t _reconnectDelayMs = 0;

  uint8_t _apChannel = 1;
  bool _apHidden = false;
  bool _loggingEnabled = true;

  IPAddress _portalIP;
  IPAddress _portalGateway;
  IPAddress _portalSubnet;

  bool _staStaticIPEnabled = false;
  IPAddress _staIP;
  IPAddress _staGateway;
  IPAddress _staSubnet;
  IPAddress _staPrimaryDNS;
  IPAddress _staSecondaryDNS;

  wifi_event_id_t _wifiEventHandlerId = 0;
  bool _coreAutoReconnectWasEnabled = false;
  std::atomic<uint32_t> _wifiEventBits{0};
  std::atomic<uint32_t> _eventDisconnectReason{0};
  uint8_t _lastDisconnectReason = 0;

  String _hostname;
  String _portalSSID;
  String _pendingSSID;
  String _pendingPassword;
  String _savedSSID;
  String _savedPassword;
  bool _credentialCacheLoaded = false;
  bool _credentialCacheValid = false;

  String _responseBuffer;
  String _scanSSID;
  String _scanCompareSSID;
  String _redirectURL;
  std::unique_ptr<ScanNetworkIdentity[]> _scanNetworkIdentities;
  size_t _scanNetworkIdentityCapacity = 0;

  String _lastError;

  Callback _onPortalStarted;
  Callback _onCredentialsSaved;
  Callback _onConnected;
};
