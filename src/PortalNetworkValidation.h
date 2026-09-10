#pragma once

#include <stdint.h>

namespace ewp_internal {

// Pure, allocation-free helpers shared by the ESP32 implementation and host
// tests. Addresses and masks use network byte order (a.b.c.d -> 0xAABBCCDD).

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
  GatewayIsBroadcastAddress
};

inline bool isUsableUnicastIPv4(uint32_t address) {
  const uint8_t firstOctet = static_cast<uint8_t>(address >> 24);
  return address != 0 && address != 0xFFFFFFFFUL && firstOctet != 0 &&
         firstOctet != 127 && firstOctet < 224;
}

inline bool isContiguousSubnetMask(uint32_t mask) {
  if (mask == 0 || mask == 0xFFFFFFFFUL) return false;
  const uint32_t hostMask = ~mask;
  return (hostMask & (hostMask + 1UL)) == 0;
}

inline uint8_t subnetPrefixLength(uint32_t mask) {
  uint8_t prefixLength = 0;
  while ((mask & 0x80000000UL) != 0) {
    ++prefixLength;
    mask <<= 1;
  }
  return prefixLength;
}

inline PortalNetworkValidationResult validatePortalNetwork(
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
  if ((local & mask) != (gateway & mask)) {
    return PortalNetworkValidationResult::DifferentSubnet;
  }

  const uint32_t hostMask = ~mask;
  const uint32_t localHost = local & hostMask;
  if (localHost == 0) {
    return PortalNetworkValidationResult::LocalIsNetworkAddress;
  }
  if (localHost == hostMask) {
    return PortalNetworkValidationResult::LocalIsBroadcastAddress;
  }

  const uint32_t gatewayHost = gateway & hostMask;
  if (gatewayHost == 0) {
    return PortalNetworkValidationResult::GatewayIsNetworkAddress;
  }
  if (gatewayHost == hostMask) {
    return PortalNetworkValidationResult::GatewayIsBroadcastAddress;
  }

  return PortalNetworkValidationResult::Valid;
}

}  // namespace ewp_internal
