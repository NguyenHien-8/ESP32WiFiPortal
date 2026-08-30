#include <ESP32WiFiPortal.h>

ESP32WiFiPortal wifiPortal;

void setup() {
  Serial.begin(115200);
  delay(200);

  wifiPortal.setHostname("esp32-device");

  // Try saved credentials. If they fail, open the captive portal.
  if (!wifiPortal.autoConnect("ESP32-Setup", "12345678", 15000, 0)) {
    Serial.print("Wi-Fi setup failed: ");
    Serial.println(wifiPortal.lastError());
    return;
  }
  
// Open your web browser and type http://192.168.4.1
  Serial.println("WiFi connected!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {}
