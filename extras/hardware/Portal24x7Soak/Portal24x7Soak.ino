#include <ESP32WiFiPortal.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

ESP32WiFiPortal wifiPortal;

constexpr uint32_t TELEMETRY_INTERVAL_MS = 60000;
constexpr uint64_t SOAK_TARGET_MS = 72ULL * 60ULL * 60ULL * 1000ULL;
constexpr size_t CREDENTIAL_RECORD_SIZE = 112;
constexpr size_t CREDENTIAL_CRC_OFFSET = 108;

uint32_t lastMillis32 = 0;
uint64_t millisEpoch = 0;
uint64_t lastTelemetryAt = 0;
uint64_t downtimeStartedAt = 0;
uint64_t totalDowntimeMs = 0;
uint32_t disconnectCount = 0;
uint32_t connectedCount = 0;
uint32_t portalStartCount = 0;
bool lastConnected = false;
bool soakTargetReported = false;

uint64_t uptimeMs() {
  const uint32_t now = millis();
  if (now < lastMillis32) millisEpoch += (1ULL << 32);
  lastMillis32 = now;
  return millisEpoch + now;
}

uint32_t crc32IEEE(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

const char* credentialIntegrity() {
  static char result[12];
  Preferences prefs;
  if (!prefs.begin("ewp_wifi", true)) return "nvs-error";
  if (!prefs.isKey("cred_blob")) {
    prefs.end();
    return "missing";
  }

  uint8_t record[CREDENTIAL_RECORD_SIZE];
  const bool exactSize =
      prefs.getBytesLength("cred_blob") == sizeof(record) &&
      prefs.getBytes("cred_blob", record, sizeof(record)) == sizeof(record);
  prefs.end();
  bool valid = exactSize;
  if (valid) {
    const uint32_t magic = static_cast<uint32_t>(record[0]) |
                           (static_cast<uint32_t>(record[1]) << 8) |
                           (static_cast<uint32_t>(record[2]) << 16) |
                           (static_cast<uint32_t>(record[3]) << 24);
    const uint16_t version = static_cast<uint16_t>(record[4]) |
                             (static_cast<uint16_t>(record[5]) << 8);
    const uint16_t size = static_cast<uint16_t>(record[6]) |
                          (static_cast<uint16_t>(record[7]) << 8);
    const uint32_t storedCRC =
        static_cast<uint32_t>(record[CREDENTIAL_CRC_OFFSET]) |
        (static_cast<uint32_t>(record[CREDENTIAL_CRC_OFFSET + 1]) << 8) |
        (static_cast<uint32_t>(record[CREDENTIAL_CRC_OFFSET + 2]) << 16) |
        (static_cast<uint32_t>(record[CREDENTIAL_CRC_OFFSET + 3]) << 24);
    valid = magic == 0x43505745UL && version == 1 &&
            size == CREDENTIAL_RECORD_SIZE && record[8] >= 1 &&
            record[8] <= 32 && record[9] <= 63 &&
            (record[9] == 0 || record[9] >= 8) &&
            storedCRC == crc32IEEE(record, CREDENTIAL_CRC_OFFSET);
  }

  volatile uint8_t* wipe = record;
  for (size_t i = 0; i < sizeof(record); ++i) wipe[i] = 0;
  snprintf(result, sizeof(result), "%s", valid ? "valid" : "invalid");
  return result;
}

void updateLinkMetrics(uint64_t now) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected == lastConnected) return;

  if (connected) {
    ++connectedCount;
    totalDowntimeMs += now - downtimeStartedAt;
  } else {
    ++disconnectCount;
    downtimeStartedAt = now;
  }
  lastConnected = connected;
}

void printTelemetry(uint64_t now) {
  const uint64_t downtime = totalDowntimeMs +
      (lastConnected ? 0 : now - downtimeStartedAt);
  Serial.printf(
      "SOAK uptime_ms=%llu wifi=%d disconnects=%lu connects=%lu reconnects=%lu "
      "downtime_ms=%llu portal_starts=%lu heap_free=%lu heap_min=%lu "
      "heap_largest=%lu reset_reason=%d credential_crc=%s\n",
      static_cast<unsigned long long>(now), static_cast<int>(WiFi.status()),
      static_cast<unsigned long>(disconnectCount),
      static_cast<unsigned long>(connectedCount),
      static_cast<unsigned long>(connectedCount > 0 ? connectedCount - 1 : 0),
      static_cast<unsigned long long>(downtime),
      static_cast<unsigned long>(portalStartCount),
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()),
      static_cast<unsigned long>(
          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
      static_cast<int>(esp_reset_reason()), credentialIntegrity());
}

void printInstructions() {
  Serial.println("24/7 soak commands: p=open portal, s=stop portal, e=erase credentials, t=telemetry, r=restart");
  Serial.println("Required manual scenarios: initial provisioning; wrong password then recovery; router off/on cycles;");
  Serial.println("repeated portal open/close; hard reset during Connect and save; then uninterrupted idle for >=72 h.");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  wifiPortal.setConnectTimeout(15000);
  wifiPortal.setConnectionRetryPolicy(3, 2000, 60000);
  wifiPortal.setAutoReconnect(true);
  wifiPortal.onPortalStarted([]() { ++portalStartCount; });

  lastMillis32 = millis();
  const uint64_t now = uptimeMs();
  lastConnected = WiFi.status() == WL_CONNECTED;
  downtimeStartedAt = now;
  printInstructions();
  printTelemetry(now);

  if (!wifiPortal.connectSaved(15000)) {
    if (!wifiPortal.startConfigPortalAsync("EWP-24x7-Soak", "12345678")) {
      Serial.print("Portal start failed: ");
      Serial.println(wifiPortal.lastError());
    }
  }
}

void loop() {
  wifiPortal.process();
  const uint64_t now = uptimeMs();
  updateLinkMetrics(now);

  if (now - lastTelemetryAt >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryAt = now;
    printTelemetry(now);
  }
  if (!soakTargetReported && now >= SOAK_TARGET_MS) {
    soakTargetReported = true;
    Serial.println("SOAK_72H_DURATION_REACHED - review all required scenario logs before declaring PASS");
  }

  if (Serial.available()) {
    switch (Serial.read()) {
      case 'p':
        wifiPortal.startConfigPortalAsync("EWP-24x7-Soak", "12345678");
        break;
      case 's':
        wifiPortal.stopConfigPortal();
        break;
      case 'e':
        wifiPortal.eraseCredentials(true);
        break;
      case 't':
        printTelemetry(now);
        break;
      case 'r':
        ESP.restart();
        break;
      default:
        break;
    }
  }
  delay(2);
}
