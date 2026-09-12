/**
 * @file ESP32WiFiPortal.h
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief ESP32 Wi-Fi captive portal library header
 * @version 2.1.2
 * @date 2026-09-12
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
  // timeoutMs == 0 is normalized to the finite 15000 ms default.
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

  // Cooperative runtime service. Call frequently; it never waits for a Wi-Fi
  // connection, disconnect settle interval, retry delay, or network scan.
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
  // Accepts usable unicast IPv4 addresses (legacy Class A/B/C ranges). The
  // one-argument overload uses localIP as gateway and a /24 subnet. Explicit
  // Portal subnets are limited to /24.../28 for Arduino-ESP32 SoftAP DHCP.
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

  // Configure before the first Wi-Fi operation. Arduino-ESP32 2.x stores at
  // most 31 hostname bytes plus the terminator.
  bool setHostname(const char* hostname);
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
#if defined(ESP32WIFIPORTAL_ENABLE_TEST_ACCESS)
  // Host tests exercise the exact private implementation used at runtime.
  friend struct ESP32WiFiPortalTestAccess;
#endif

  enum class PortalNetworkValidationResult : uint8_t {
    Valid,
    InvalidLocalIP,
    InvalidGateway,
    InvalidSubnetMask,
    UnsupportedSubnet,
    DifferentSubnet,
    LocalIsNetworkAddress,
    LocalIsBroadcastAddress,
    GatewayIsNetworkAddress,
    GatewayIsBroadcastAddress,
    LocalConflictsWithDHCPLease,
    GatewayConflictsWithDHCPLease
  };

  enum class ConnectionOwner : uint8_t {
    None,
    Blocking,
    Portal,
    Reconnect
  };

  enum class ConnectionPhase : uint8_t {
    Idle,
    Settling,
    Connecting
  };

  enum class ScanState : uint8_t {
    Idle,
    Scanning,
    Ready,
    Failed
  };

  enum class CredentialCacheStatus : uint8_t {
    Unknown,
    Valid,
    NotFound,
    Unavailable,
    Corrupt
  };

  struct ScanNetworkIdentity {
    uint32_t hash;
    int index;
  };

  static constexpr uint16_t kDnsPort = 53;
  static constexpr uint16_t kHttpPort = 80;
  static constexpr const char* kPrefsNamespace = "ewp_wifi";
  static constexpr const char* kPrefsCredential = "cred_blob";
  static constexpr const char* kPrefsCredentialBackup = "cred_backup";
  static constexpr const char* kPrefsCredentialEraseMarker = "cred_erased";
  static constexpr const char* kPrefsSSID = "ssid";
  static constexpr const char* kPrefsPassword = "pass";
  static constexpr uint32_t kCredentialMagic = 0x43505745UL;  // "EWPC"
  static constexpr uint16_t kCredentialVersion = 1;
  static constexpr size_t kCredentialSSIDCapacity = 33;
  static constexpr size_t kCredentialPasswordCapacity = 65;
  static constexpr size_t kCredentialSSIDOffset = 10;
  static constexpr size_t kCredentialPasswordOffset =
      kCredentialSSIDOffset + kCredentialSSIDCapacity;
  static constexpr size_t kCredentialCRCOffset =
      kCredentialPasswordOffset + kCredentialPasswordCapacity;
  static constexpr size_t kCredentialRecordSize = kCredentialCRCOffset + 4;
  static constexpr uint32_t kDefaultConnectTimeoutMs = 15000;
  static_assert(kCredentialRecordSize == 112,
                "Credential record layout changed unexpectedly");
  static constexpr uint32_t kEventSTAConnected = 1UL << 0;
  static constexpr uint32_t kEventSTAGotIP = 1UL << 1;
  static constexpr uint32_t kEventSTADisconnected = 1UL << 2;
  static constexpr uint32_t kSTADisconnectSettleMs = 20;
  static constexpr uint32_t kScanTimeoutMs = 15000;
  static constexpr uint32_t kRestartDelayMs = 350;

  // Allocation-free IPv4 helpers live in the class so Portal and STA policy
  // share only their low-level primitives. Definitions inside the class are
  // implicitly inline, making this header safe in multiple translation units.
  static inline uint32_t ipv4ToUint32(const IPAddress& address) {
    return (static_cast<uint32_t>(address[0]) << 24) |
           (static_cast<uint32_t>(address[1]) << 16) |
           (static_cast<uint32_t>(address[2]) << 8) |
           static_cast<uint32_t>(address[3]);
  }

  static inline bool isUsableUnicastIPv4(uint32_t address) {
    const uint8_t firstOctet = static_cast<uint8_t>(address >> 24);
    return address != 0 && address != 0xFFFFFFFFUL && firstOctet != 0 &&
           firstOctet != 127 && firstOctet < 224;
  }

  static inline bool isContiguousSubnetMask(uint32_t mask) {
    if (mask == 0 || mask == 0xFFFFFFFFUL) return false;
    const uint32_t hostMask = ~mask;
    return (hostMask & (hostMask + 1UL)) == 0;
  }

  static inline uint8_t subnetPrefixLength(uint32_t mask) {
    uint8_t prefixLength = 0;
    while ((mask & 0x80000000UL) != 0) {
      ++prefixLength;
      mask <<= 1;
    }
    return prefixLength;
  }

  static inline bool isSameSubnet(uint32_t first,
                                  uint32_t second,
                                  uint32_t mask) {
    return (first & mask) == (second & mask);
  }

  static inline uint32_t networkAddress(uint32_t address, uint32_t mask) {
    return address & mask;
  }

  static inline uint32_t broadcastAddress(uint32_t address, uint32_t mask) {
    return networkAddress(address, mask) | ~mask;
  }

  static inline uint32_t defaultDHCPLeaseStart(uint32_t local,
                                               uint32_t mask) {
    const uint32_t hostMask = ~mask;
    const uint32_t candidate = local + 1UL;
    // Arduino-ESP32 keeps eleven inclusive addresses available to its SoftAP
    // DHCP server and moves an overflowing default range to network + 1.
    return ((candidate & hostMask) >= hostMask - 10UL)
               ? networkAddress(local, mask) + 1UL
               : candidate;
  }

  static inline bool isInInclusiveRange(uint32_t address,
                                        uint32_t first,
                                        uint32_t last) {
    return address >= first && address <= last;
  }

  static inline PortalNetworkValidationResult validatePortalNetwork(
      uint32_t local,
      uint32_t gateway,
      uint32_t mask) {
    if (!isUsableUnicastIPv4(local)) {
      return PortalNetworkValidationResult::InvalidLocalIP;
    }
    if (!isUsableUnicastIPv4(gateway)) {
      return PortalNetworkValidationResult::InvalidGateway;
    }
    if (!isContiguousSubnetMask(mask)) {
      return PortalNetworkValidationResult::InvalidSubnetMask;
    }

    const uint8_t prefixLength = subnetPrefixLength(mask);
    if (prefixLength < 24 || prefixLength > 28) {
      return PortalNetworkValidationResult::UnsupportedSubnet;
    }
    if (!isSameSubnet(local, gateway, mask)) {
      return PortalNetworkValidationResult::DifferentSubnet;
    }

    const uint32_t network = networkAddress(local, mask);
    const uint32_t broadcast = broadcastAddress(local, mask);
    if (local == network) {
      return PortalNetworkValidationResult::LocalIsNetworkAddress;
    }
    if (local == broadcast) {
      return PortalNetworkValidationResult::LocalIsBroadcastAddress;
    }
    if (gateway == network) {
      return PortalNetworkValidationResult::GatewayIsNetworkAddress;
    }
    if (gateway == broadcast) {
      return PortalNetworkValidationResult::GatewayIsBroadcastAddress;
    }

    // Match the default lease-range selection in Arduino-ESP32 3.x. This
    // prevents setPortalIP() from accepting a host/gateway that AP.config()
    // would later reject because it overlaps the DHCP pool.
    const uint32_t leaseStart = defaultDHCPLeaseStart(local, mask);
    const uint32_t leaseEnd = leaseStart + 10UL;
    if (isInInclusiveRange(local, leaseStart, leaseEnd)) {
      return PortalNetworkValidationResult::LocalConflictsWithDHCPLease;
    }
    if (isInInclusiveRange(gateway, leaseStart, leaseEnd)) {
      return PortalNetworkValidationResult::GatewayConflictsWithDHCPLease;
    }

    return PortalNetworkValidationResult::Valid;
  }

  // STA static addressing intentionally has no SoftAP /24.../28 restriction.
  static inline bool isValidSTANetwork(uint32_t local,
                                       uint32_t gateway,
                                       uint32_t mask) {
    if (!isUsableUnicastIPv4(local) ||
        !isUsableUnicastIPv4(gateway) ||
        !isContiguousSubnetMask(mask) ||
        !isSameSubnet(local, gateway, mask)) {
      return false;
    }
    const uint32_t network = networkAddress(local, mask);
    const uint32_t broadcast = broadcastAddress(local, mask);
    return local != network && local != broadcast && gateway != network &&
           gateway != broadcast;
  }

  static inline bool isValidDNSAddress(uint32_t address) {
    return address == 0 || isUsableUnicastIPv4(address);
  }

  static const char* portalNetworkValidationMessage(
      PortalNetworkValidationResult result);

  bool openPortal(const char* apSSID, const char* apPassword, uint32_t portalTimeoutMs);
  bool failPortalStart(const char* message);
  void configureRoutes();
  void handleRoot();
  void handleScan();
  void handleSave();
  void handleStatus();
  void handleProperties();
  void handleReset();
  void handleNotFound();
  void handleCaptiveProbe();
  void processScan();
  void resetScan(bool cancelActiveScan);
  void beginPendingConnection();
  void clearPendingConnection(bool disconnectSTA);
  void failPendingConnection(bool terminalFailure);

  bool connect(uint32_t timeoutMs);
  bool beginSTAConnection(ConnectionOwner owner);
  bool advanceSTAConnection();
  bool applySTAConfig();
  void releaseSTAConnection();
  void cancelSTAConnection();
  bool ensureWiFiEventHandler();
  void processWiFiEvents();
  void processAutoReconnect();
  void scheduleAutoReconnect(uint32_t delayMs);
  void scheduleNextAutoReconnect();
  void cancelAutoReconnect(bool disconnectSTA);
  bool scheduleSavedConnectionRecovery();
  uint32_t retryDelay(uint8_t retryNumber) const;
  bool saveCredentials(const String& ssid, const String& password);
  bool ensureCredentialCache();
  static bool validSTACredentials(const String& ssid,
                                  const String& password);
  static uint32_t credentialCRC32(const uint8_t* data, size_t length);
  static bool serializeCredentialRecord(const String& ssid,
                                        const String& password,
                                        uint8_t* record,
                                        size_t recordSize);
  static bool deserializeCredentialRecord(const uint8_t* record,
                                          size_t recordSize,
                                          String& ssid,
                                          String& password);
  static void secureClear(void* data, size_t length);
  CredentialCacheStatus readCredentialBlob(Preferences& prefs,
                                           const char* key,
                                           String& ssid,
                                           String& password);
  CredentialCacheStatus readLegacyCredentials(Preferences& prefs,
                                               String& ssid,
                                               String& password);
  bool writeCredentialRecord(Preferences& prefs,
                             const String& ssid,
                             const String& password);
  bool writeCredentialBytes(Preferences& prefs,
                            const char* key,
                            const uint8_t* record,
                            const String& expectedSSID,
                            const String& expectedPassword);
  static bool writeCredentialEraseMarker(Preferences& prefs);
  static bool removeStoredCredentials(Preferences& prefs);
  void clearCredentialCache(CredentialCacheStatus status);
  static bool isValidRawPSK(const String& password);
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
  ConnectionPhase _connectionPhase = ConnectionPhase::Idle;
  bool _portalActive = false;
  bool _connectPending = false;
  bool _connectAttemptActive = false;
  bool _attemptTerminalFailure = false;
  bool _staDisconnected = false;
  bool _restartPending = false;

  uint32_t _connectTimeoutMs = kDefaultConnectTimeoutMs;
  uint32_t _portalTimeoutMs = 0;
  uint32_t _portalStartedAt = 0;
  uint32_t _connectPendingAt = 0;
  uint32_t _connectAttemptAt = 0;
  uint32_t _connectPendingDelayMs = 350;
  uint32_t _connectionPhaseAt = 0;
  uint32_t _connectionSettleDelayMs = 0;
  uint32_t _restartRequestedAt = 0;

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
  std::atomic<uint32_t> _eventCredentialFailureReason{0};
  uint8_t _lastDisconnectReason = 0;

  String _hostname;
  String _portalSSID;
  String _pendingSSID;
  String _pendingPassword;
  String _savedSSID;
  String _savedPassword;
  CredentialCacheStatus _credentialCacheStatus =
      CredentialCacheStatus::Unknown;

  String _responseBuffer;
  String _scanSSID;
  String _scanCompareSSID;
  String _redirectURL;
  std::unique_ptr<ScanNetworkIdentity[]> _scanNetworkIdentities;
  size_t _scanNetworkIdentityCapacity = 0;
  ScanState _scanState = ScanState::Idle;
  uint32_t _scanStartedAt = 0;
  int _scanResultCount = 0;

  String _lastError;

  Callback _onPortalStarted;
  Callback _onCredentialsSaved;
  Callback _onConnected;
};
