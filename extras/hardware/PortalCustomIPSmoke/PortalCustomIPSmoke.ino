#include <ESP32WiFiPortal.h>
#include <esp_heap_caps.h>

ESP32WiFiPortal wifiPortal;

constexpr uint16_t STRESS_CYCLES = 100;
constexpr size_t MAX_ALLOWED_HEAP_DRIFT = 2048;

void fail(const char* message) {
  Serial.print("FAIL: ");
  Serial.println(message);
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  wifiPortal.setLogging(false);

  if (!wifiPortal.setPortalIP(IPAddress(200, 5, 29, 8))) {
    fail(wifiPortal.lastError().c_str());
  }

  const size_t initialHeap = ESP.getFreeHeap();
  for (uint16_t cycle = 0; cycle < STRESS_CYCLES; ++cycle) {
    if (!wifiPortal.startConfigPortalAsync("EWP-v2.1.2-Test", "12345678")) {
      fail(wifiPortal.lastError().c_str());
    }
    if (WiFi.softAPIP() != IPAddress(200, 5, 29, 8)) {
      fail("SoftAP runtime IP mismatch");
    }
    wifiPortal.process();
    wifiPortal.stopConfigPortal();
    if (wifiPortal.isPortalActive()) fail("Portal did not stop");
  }

  const size_t finalHeap = ESP.getFreeHeap();
  if (initialHeap > finalHeap && initialHeap - finalHeap > MAX_ALLOWED_HEAP_DRIFT) {
    fail("Heap drift exceeded 2048 bytes");
  }

  if (!wifiPortal.startConfigPortalAsync("EWP-v2.1.2-Test", "12345678")) {
    fail(wifiPortal.lastError().c_str());
  }

  Serial.println("AUTOMATED CHECKS PASS");
  Serial.print("Portal URL: http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Minimum free heap: ");
  Serial.println(ESP.getMinFreeHeap());
  Serial.print("Largest free block: ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.println("Connect a client and verify DHCP, DNS wildcard, captive probes,");
  Serial.println("/, /scan, /save, /status, /properties and POST /reset.");
}

void loop() {
  wifiPortal.process();
  delay(2);
}
