/**
 * @file Basic.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating the usage of ESP32WiFiPortal library.
 * @version 2.1.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);
  delay(200);

  wifiPortal.setHostname("esp32-device");
  wifiPortal.onPortalStarted([]() {
    Serial.print("Setup portal: http://");
    Serial.println(wifiPortal.portalIP());  // Defaults to 192.168.4.1
  });

  // Try saved credentials. If they fail, open the captive portal.
  if (!wifiPortal.autoConnect("ESP32-Setup", "12345678", 15000, 0)) {
    Serial.print("Wi-Fi setup failed: ");
    Serial.println(wifiPortal.lastError());
    return;
  }
  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  wifiPortal.process();
  delay(2);
}
