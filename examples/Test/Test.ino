/**
 * @file Test.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating the usage of ESP32WiFiPortal library.
 * @version 1.1.1
 * @date 2026-08-31

 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Delete Wi-Fi credentials stored in NVS.
  wifiPortal.eraseCredentials(true);

  if (!wifiPortal.autoConnect(
        "ESP32-Setup",
        "12345678",
        15000,
        0)) {

    Serial.print("WiFi setup failed: ");
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