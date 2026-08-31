/**
 * @file CustomIP.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating the usage of ESP32WiFiPortal library with custom portal IP address.
 * @version 1.1.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);

  // Must be configured before the portal starts. Only valid private IPv4
  // host addresses are accepted. Gateway defaults to the same address and
  // subnet defaults to 255.255.255.0.
  if (!wifiPortal.setPortalIP(IPAddress(192, 168, 50, 1))) {
    Serial.println(wifiPortal.lastError());
    return;
  }

  wifiPortal.onPortalStarted([]() {
    Serial.print("Custom setup address: http://");
    Serial.println(wifiPortal.portalIP());
  });

  if (!wifiPortal.autoConnect("ESP32-Setup", "12345678")) {
    Serial.println(wifiPortal.lastError());
  }
}

void loop() {
  wifiPortal.process();
  delay(2);
}
