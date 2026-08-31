/**
 * @file OnDemand.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Example sketch demonstrating on-demand Wi-Fi configuration portal using ESP32WiFiPortal library.
 * @version 1.1.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#include <ESP32WiFiPortal.h>

constexpr uint8_t CONFIG_BUTTON_PIN = 35;  // Active LOW example
constexpr uint32_t HOLD_TIME_MS = 3000;

ESP32WiFiPortal wifiPortal;
uint32_t pressedAt = 0;
bool handled = false;

void setup() {
  Serial.begin(115200);
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  if (!wifiPortal.connectSaved(15000)) {
    Serial.println("Saved Wi-Fi unavailable. Device remains offline.");
  }
}

void loop() {
  wifiPortal.process();

  const bool pressed = digitalRead(CONFIG_BUTTON_PIN) == LOW;

  if (pressed && pressedAt == 0) {
    pressedAt = millis();
    handled = false;
  }

  if (pressed && !handled && millis() - pressedAt >= HOLD_TIME_MS) {
    handled = true;
    Serial.println("Opening Wi-Fi configuration portal...");

    // Blocking portal. Change the AP password for production use.
    if (wifiPortal.startConfigPortal("ESP32-Setup", "12345678", 300000)) {
      Serial.println("Connected to the selected Wi-Fi network.");
    } else {
      Serial.print("Portal ended: ");
      Serial.println(wifiPortal.lastError());
    }
  }

  if (!pressed) pressedAt = 0;
  delay(5);
}
