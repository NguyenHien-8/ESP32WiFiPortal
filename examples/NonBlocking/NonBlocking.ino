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
  // Required only while using the async/non-blocking portal.
  wifiPortal.process();

  // Your application code can continue running here.
  delay(2);
}
