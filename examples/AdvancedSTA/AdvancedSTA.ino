#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);

  // Replace these values with valid addresses for your LAN. This affects only
  // the STA interface; the Config Portal keeps its own independent address.
  if (!wifiPortal.setSTAStaticIP(
          IPAddress(192, 168, 1, 50),
          IPAddress(192, 168, 1, 1),
          IPAddress(255, 255, 255, 0),
          IPAddress(1, 1, 1, 1),
          IPAddress(8, 8, 8, 8))) {
    Serial.println(wifiPortal.lastError());
    return;
  }

  wifiPortal.setConnectTimeout(10000);
  if (!wifiPortal.setConnectionRetryPolicy(3, 2000, 60000)) {
    Serial.println(wifiPortal.lastError());
    return;
  }

  if (!wifiPortal.autoConnect("ESP32-Setup", "12345678")) {
    Serial.println(wifiPortal.lastError());
    return;
  }

  // Auto Reconnect is enabled by default; this explicit call documents intent.
  // It is driven by process() in loop().
  wifiPortal.setAutoReconnect(true);
}

void loop() {
  wifiPortal.process();
  delay(2);
}
