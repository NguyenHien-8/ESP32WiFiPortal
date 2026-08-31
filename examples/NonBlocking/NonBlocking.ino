/**
 * @file NonBlocking.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating non-blocking Wi-Fi configuration portal using ESP32WiFiPortal library.
 * @version 1.1.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);

  wifiPortal.onPortalStarted([]() {
    Serial.println("Portal started");
  });

  wifiPortal.onCredentialsSaved([]() {
    Serial.println("Credentials saved");
  });

  wifiPortal.onConnected([]() {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  });

  if (!wifiPortal.connectSaved(10000)) {
    wifiPortal.startConfigPortalAsync("ESP32-Setup", "12345678", 300000);
  }
}

void loop() {
  // Required for the async portal and library-managed Auto Reconnect.
  // Keep this loop cooperative: schedule long-running application work with
  // millis() and avoid long delay() or blocking network calls.
  wifiPortal.process();

  // Your application code can continue running here.
  delay(2);
}
