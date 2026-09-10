#pragma once

#include <ESP32WiFiPortal.h>

struct ESP32WiFiPortalTestAccess {
  using Result = ESP32WiFiPortal::PortalNetworkValidationResult;

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
};
