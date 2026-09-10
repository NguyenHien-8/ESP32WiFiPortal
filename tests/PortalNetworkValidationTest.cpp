#include "../src/PortalNetworkValidation.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

constexpr uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return (static_cast<uint32_t>(a) << 24) |
         (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(c) << 8) |
         static_cast<uint32_t>(d);
}

using Result = ewp_internal::PortalNetworkValidationResult;

void expect(Result expected,
            uint32_t local,
            uint32_t gateway,
            uint32_t subnet) {
  assert(ewp_internal::validatePortalNetwork(local, gateway, subnet) ==
         expected);
}

}  // namespace

int main() {
  const uint32_t slash23 = ipv4(255, 255, 254, 0);
  const uint32_t slash24 = ipv4(255, 255, 255, 0);
  const uint32_t slash25 = ipv4(255, 255, 255, 128);
  const uint32_t slash26 = ipv4(255, 255, 255, 192);
  const uint32_t slash27 = ipv4(255, 255, 255, 224);
  const uint32_t slash28 = ipv4(255, 255, 255, 240);
  const uint32_t slash29 = ipv4(255, 255, 255, 248);

  // Required PASS matrix: legacy Class A, B, C, plus the previous RFC1918 case.
  expect(Result::Valid, ipv4(10, 10, 0, 1), ipv4(10, 10, 0, 1), slash24);
  expect(Result::Valid, ipv4(150, 10, 20, 1), ipv4(150, 10, 20, 1), slash24);
  expect(Result::Valid, ipv4(200, 5, 29, 8), ipv4(200, 5, 29, 8), slash24);
  expect(Result::Valid, ipv4(192, 168, 50, 1), ipv4(192, 168, 50, 1), slash24);
  expect(Result::Valid, ipv4(200, 5, 29, 1), ipv4(200, 5, 29, 1), slash25);
  expect(Result::Valid, ipv4(200, 5, 29, 1), ipv4(200, 5, 29, 1), slash26);
  expect(Result::Valid, ipv4(200, 5, 29, 1), ipv4(200, 5, 29, 1), slash27);
  expect(Result::Valid, ipv4(200, 5, 29, 1), ipv4(200, 5, 29, 1), slash28);

  // Required safe failures.
  expect(Result::InvalidLocalIP, ipv4(0, 1, 2, 3), ipv4(0, 1, 2, 3), slash24);
  expect(Result::InvalidLocalIP, ipv4(127, 0, 0, 1), ipv4(127, 0, 0, 1), slash24);
  expect(Result::InvalidLocalIP, ipv4(224, 0, 0, 1), ipv4(224, 0, 0, 1), slash24);
  expect(Result::InvalidLocalIP, ipv4(255, 255, 255, 255),
         ipv4(255, 255, 255, 255), slash24);
  expect(Result::LocalIsNetworkAddress, ipv4(200, 5, 29, 0),
         ipv4(200, 5, 29, 1), slash24);
  expect(Result::LocalIsBroadcastAddress, ipv4(200, 5, 29, 255),
         ipv4(200, 5, 29, 1), slash24);
  expect(Result::DifferentSubnet, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 30, 1), slash24);
  expect(Result::InvalidSubnetMask, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 8), ipv4(255, 0, 255, 0));
  expect(Result::InvalidSubnetMask, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 8), ipv4(0, 0, 0, 0));
  expect(Result::InvalidSubnetMask, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 8), ipv4(255, 255, 255, 255));
  expect(Result::UnsupportedSubnet, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 8), slash23);
  expect(Result::UnsupportedSubnet, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 8), slash29);

  // Gateway-specific diagnostics and the supported range boundaries.
  expect(Result::InvalidGateway, ipv4(200, 5, 29, 8),
         ipv4(127, 0, 0, 1), slash24);
  expect(Result::GatewayIsNetworkAddress, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 0), slash24);
  expect(Result::GatewayIsBroadcastAddress, ipv4(200, 5, 29, 8),
         ipv4(200, 5, 29, 255), slash24);

  std::cout << "Portal IPv4 validation tests passed\n";
  return 0;
}
