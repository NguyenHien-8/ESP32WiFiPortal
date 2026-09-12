/**
 * @file CustomIP.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating the usage of ESP32WiFiPortal library with custom portal IP address.
 * @version 2.1.2
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);

  // Configure before the portal starts. Usable unicast addresses from the
  // legacy Class A/B/C ranges are accepted; 10.10.0.1, 150.10.20.1, and
  // 200.5.29.8 are examples. The one-argument overload uses the same address
  // as gateway and a /24 subnet.
  // ---------------------------------------------------------
  // Configure Captive Portal IP
  // Portal IP : 200.5.29.8
  // Gateway   : 200.5.29.8
  // Subnet    : 255.255.255.0 (/24)
  //
  // ---------------------------------------------------------
  if (!wifiPortal.setPortalIP(IPAddress(200, 5, 29, 8))) {
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
