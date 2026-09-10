#include "host/TestAccess.h"

bool headerODRPartB() {
  const uint32_t slash24 = ESP32WiFiPortalTestAccess::ipv4(255, 255, 255, 0);
  return ESP32WiFiPortalTestAccess::validate(
             ESP32WiFiPortalTestAccess::ipv4(200, 5, 29, 8),
             ESP32WiFiPortalTestAccess::ipv4(200, 5, 29, 8), slash24) ==
         ESP32WiFiPortalTestAccess::Result::Valid;
}
