#include "host/TestAccess.h"

bool headerODRPartA() {
  const uint32_t slash24 = ESP32WiFiPortalTestAccess::ipv4(255, 255, 255, 0);
  return ESP32WiFiPortalTestAccess::validate(
             ESP32WiFiPortalTestAccess::ipv4(10, 10, 0, 1),
             ESP32WiFiPortalTestAccess::ipv4(10, 10, 0, 1), slash24) ==
         ESP32WiFiPortalTestAccess::Result::Valid;
}
