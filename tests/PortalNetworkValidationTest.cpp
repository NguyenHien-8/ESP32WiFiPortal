#include "host/TestAccess.h"

#include "host/TestAssert.h"
#include <iostream>

namespace {

using Access = ESP32WiFiPortalTestAccess;
using Result = Access::Result;

void expect(Result expected,
            uint32_t local,
            uint32_t gateway,
            uint32_t subnet) {
  assert(Access::validate(local, gateway, subnet) == expected);
}

}  // namespace

int main() {
  const uint32_t slash23 = Access::ipv4(255, 255, 254, 0);
  const uint32_t slash24 = Access::ipv4(255, 255, 255, 0);
  const uint32_t slash25 = Access::ipv4(255, 255, 255, 128);
  const uint32_t slash26 = Access::ipv4(255, 255, 255, 192);
  const uint32_t slash27 = Access::ipv4(255, 255, 255, 224);
  const uint32_t slash28 = Access::ipv4(255, 255, 255, 240);
  const uint32_t slash29 = Access::ipv4(255, 255, 255, 248);

  assert(Access::usable(Access::ipv4(10, 10, 0, 1)));
  assert(Access::usable(Access::ipv4(150, 10, 20, 1)));
  assert(Access::usable(Access::ipv4(200, 5, 29, 8)));
  assert(!Access::usable(Access::ipv4(0, 0, 0, 0)));
  assert(!Access::usable(Access::ipv4(127, 0, 0, 1)));
  assert(!Access::usable(Access::ipv4(224, 0, 0, 1)));
  assert(!Access::usable(Access::ipv4(255, 255, 255, 255)));

  assert(Access::contiguous(slash24));
  assert(Access::contiguous(slash25));
  assert(Access::contiguous(slash26));
  assert(Access::contiguous(slash27));
  assert(Access::contiguous(slash28));
  assert(!Access::contiguous(Access::ipv4(255, 0, 255, 0)));
  assert(Access::prefix(slash24) == 24);
  assert(Access::prefix(slash28) == 28);

  assert(Access::sameSubnet(Access::ipv4(200, 5, 29, 8),
                            Access::ipv4(200, 5, 29, 1), slash24));
  assert(!Access::sameSubnet(Access::ipv4(200, 5, 29, 8),
                             Access::ipv4(200, 5, 30, 1), slash24));
  assert(Access::network(Access::ipv4(200, 5, 29, 8), slash24) ==
         Access::ipv4(200, 5, 29, 0));
  assert(Access::broadcast(Access::ipv4(200, 5, 29, 8), slash24) ==
         Access::ipv4(200, 5, 29, 255));

  // Required PASS matrix: legacy Class A, B, C and the RFC1918 case.
  expect(Result::Valid, Access::ipv4(10, 10, 0, 1),
         Access::ipv4(10, 10, 0, 1), slash24);
  expect(Result::Valid, Access::ipv4(150, 10, 20, 1),
         Access::ipv4(150, 10, 20, 1), slash24);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), slash24);
  expect(Result::Valid, Access::ipv4(192, 168, 50, 1),
         Access::ipv4(192, 168, 50, 1), slash24);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 1),
         Access::ipv4(200, 5, 29, 1), slash25);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 1),
         Access::ipv4(200, 5, 29, 1), slash26);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 1),
         Access::ipv4(200, 5, 29, 1), slash27);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 1),
         Access::ipv4(200, 5, 29, 1), slash28);

  // Required safe failures.
  expect(Result::InvalidLocalIP, Access::ipv4(0, 0, 0, 0),
         Access::ipv4(1, 1, 1, 1), slash24);
  expect(Result::InvalidLocalIP, Access::ipv4(0, 1, 2, 3),
         Access::ipv4(0, 1, 2, 3), slash24);
  expect(Result::InvalidLocalIP, Access::ipv4(127, 0, 0, 1),
         Access::ipv4(127, 0, 0, 1), slash24);
  expect(Result::InvalidLocalIP, Access::ipv4(224, 0, 0, 1),
         Access::ipv4(224, 0, 0, 1), slash24);
  expect(Result::InvalidLocalIP, Access::ipv4(255, 255, 255, 255),
         Access::ipv4(255, 255, 255, 255), slash24);
  expect(Result::LocalIsNetworkAddress, Access::ipv4(200, 5, 29, 0),
         Access::ipv4(200, 5, 29, 1), slash24);
  expect(Result::LocalIsBroadcastAddress, Access::ipv4(200, 5, 29, 255),
         Access::ipv4(200, 5, 29, 1), slash24);
  expect(Result::DifferentSubnet, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 30, 1), slash24);
  expect(Result::InvalidSubnetMask, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), Access::ipv4(255, 0, 255, 0));
  expect(Result::InvalidSubnetMask, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), Access::ipv4(0, 0, 0, 0));
  expect(Result::InvalidSubnetMask, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), Access::ipv4(255, 255, 255, 255));
  expect(Result::UnsupportedSubnet, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), slash23);
  expect(Result::UnsupportedSubnet, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 8), slash29);
  expect(Result::InvalidGateway, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(127, 0, 0, 1), slash24);
  expect(Result::GatewayIsNetworkAddress, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 0), slash24);
  expect(Result::GatewayIsBroadcastAddress, Access::ipv4(200, 5, 29, 8),
         Access::ipv4(200, 5, 29, 255), slash24);
  expect(Result::LocalConflictsWithDHCPLease,
         Access::ipv4(200, 5, 29, 8), Access::ipv4(200, 5, 29, 8), slash28);
  expect(Result::GatewayConflictsWithDHCPLease,
         Access::ipv4(200, 5, 29, 12), Access::ipv4(200, 5, 29, 1), slash28);
  expect(Result::Valid, Access::ipv4(200, 5, 29, 12),
         Access::ipv4(200, 5, 29, 12), slash28);

  // Public API uses the same implementation and preserves the STA /16 policy.
  ESP32WiFiPortal portal;
  portal.setLogging(false);
  assert(portal.setPortalIP(IPAddress(10, 10, 0, 1)));
  assert(portal.setPortalIP(IPAddress(150, 10, 20, 1)));
  assert(portal.setPortalIP(IPAddress(200, 5, 29, 8)));
  assert(portal.setPortalIP(IPAddress(192, 168, 50, 1)));
  assert(!portal.setPortalIP(IPAddress(200, 5, 29, 0)));
  assert(!portal.setPortalIP(IPAddress(200, 5, 29, 8),
                             IPAddress(200, 5, 29, 8),
                             IPAddress(255, 255, 255, 240)));
  assert(portal.setSTAStaticIP(IPAddress(150, 10, 20, 50),
                               IPAddress(150, 10, 0, 1),
                               IPAddress(255, 255, 0, 0)));

  std::cout << "Portal IPv4 validation and setPortalIP tests passed\n";
  return 0;
}
