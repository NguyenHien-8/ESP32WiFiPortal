#pragma once

#include <ESP32WiFiPortal.h>

struct ESP32WiFiPortalTestAccess {
  using Result = ESP32WiFiPortal::PortalNetworkValidationResult;
  using CredentialStatus = ESP32WiFiPortal::CredentialCacheStatus;

  static uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
  }

  static Result validate(uint32_t local, uint32_t gateway, uint32_t mask) {
    return ESP32WiFiPortal::validatePortalNetwork(local, gateway, mask);
  }

  static bool usable(uint32_t address) {
    return ESP32WiFiPortal::isUsableUnicastIPv4(address);
  }

  static bool contiguous(uint32_t mask) {
    return ESP32WiFiPortal::isContiguousSubnetMask(mask);
  }

  static uint8_t prefix(uint32_t mask) {
    return ESP32WiFiPortal::subnetPrefixLength(mask);
  }

  static bool sameSubnet(uint32_t first, uint32_t second, uint32_t mask) {
    return ESP32WiFiPortal::isSameSubnet(first, second, mask);
  }

  static uint32_t network(uint32_t address, uint32_t mask) {
    return ESP32WiFiPortal::networkAddress(address, mask);
  }

  static uint32_t broadcast(uint32_t address, uint32_t mask) {
    return ESP32WiFiPortal::broadcastAddress(address, mask);
  }

  static const String& redirectURL(const ESP32WiFiPortal& portal) {
    return portal._redirectURL;
  }

  static size_t credentialRecordSize() {
    return ESP32WiFiPortal::kCredentialRecordSize;
  }

  static uint32_t credentialCRC32(const uint8_t* data, size_t length) {
    return ESP32WiFiPortal::credentialCRC32(data, length);
  }

  static bool serializeCredentials(const String& ssid,
                                   const String& password,
                                   uint8_t* record,
                                   size_t size) {
    return ESP32WiFiPortal::serializeCredentialRecord(
        ssid, password, record, size);
  }

  static bool deserializeCredentials(const uint8_t* record,
                                     size_t size,
                                     String& ssid,
                                     String& password) {
    return ESP32WiFiPortal::deserializeCredentialRecord(
        record, size, ssid, password);
  }

  static bool saveCredentials(ESP32WiFiPortal& portal,
                              const String& ssid,
                              const String& password) {
    return portal.saveCredentials(ssid, password);
  }

  static CredentialStatus credentialStatus(const ESP32WiFiPortal& portal) {
    return portal._credentialCacheStatus;
  }

  static const String& savedPassword(const ESP32WiFiPortal& portal) {
    return portal._savedPassword;
  }

  static const String& pendingSSID(const ESP32WiFiPortal& portal) {
    return portal._pendingSSID;
  }

  static uint32_t connectTimeout(const ESP32WiFiPortal& portal) {
    return portal._connectTimeoutMs;
  }

  static bool reconnectScheduled(const ESP32WiFiPortal& portal) {
    return portal._reconnectScheduled;
  }

  static uint8_t reconnectRetriesUsed(const ESP32WiFiPortal& portal) {
    return portal._reconnectRetriesUsed;
  }

  static uint32_t reconnectDelay(const ESP32WiFiPortal& portal) {
    return portal._reconnectDelayMs;
  }

  static uint32_t connectAttemptAt(const ESP32WiFiPortal& portal) {
    return portal._connectAttemptAt;
  }

  static bool reconnectAttemptActive(const ESP32WiFiPortal& portal) {
    return portal._connectAttemptActive &&
           portal._connectionOwner == ESP32WiFiPortal::ConnectionOwner::Reconnect;
  }
};
