/**
 * @file OnDemandFromSerialMonitor.ino
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief Open the ESP32 Wi-Fi Config Portal on demand from Serial Monitor.
 * @version 1.1.1
 * @date 2026-08-31
 *
 * Type PORTAL in Serial Monitor and press Enter to open the Config Portal.
 * Use "Newline" or "Both NL & CR" as the Serial Monitor line ending.
 */

#include <ESP32WiFiPortal.h>
#include <ctype.h>
#include <string.h>

constexpr char PORTAL_SSID[] = "ESP32-Setup";
constexpr char PORTAL_PASSWORD[] = "12345678";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t PORTAL_TIMEOUT_MS = 300000;  // 5 minutes

ESP32WiFiPortal wifiPortal;

char serialCommand[24];
size_t serialCommandLength = 0;
bool portalWasActive = false;

void openConfigPortal() {
  if (wifiPortal.isPortalActive()) {
    Serial.println("Config Portal is already running.");
    return;
  }

  Serial.println("Opening Wi-Fi Config Portal...");

  if (!wifiPortal.startConfigPortalAsync(
          PORTAL_SSID,
          PORTAL_PASSWORD,
          PORTAL_TIMEOUT_MS)) {
    Serial.print("Unable to start Config Portal: ");
    Serial.println(wifiPortal.lastError());
    return;
  }

  portalWasActive = true;
  Serial.print("Connect to AP: ");
  Serial.println(PORTAL_SSID);
  Serial.print("Open: http://");
  Serial.println(wifiPortal.portalIP());
}

void handleSerialCommand(const char* command) {
  if (strcmp(command, "PORTAL") == 0) {
    openConfigPortal();
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
  Serial.println("Type PORTAL and press Enter.");
}

void processSerial() {
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());

    if (received == '\r' || received == '\n') {
      if (serialCommandLength == 0) continue;

      serialCommand[serialCommandLength] = '\0';
      handleSerialCommand(serialCommand);
      serialCommandLength = 0;
      continue;
    }

    if (serialCommandLength < sizeof(serialCommand) - 1) {
      serialCommand[serialCommandLength++] =
          static_cast<char>(toupper(static_cast<unsigned char>(received)));
    } else {
      // Drop an overlong command without allocating dynamic memory.
      serialCommandLength = 0;
      Serial.println("Command too long. Type PORTAL and press Enter.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  wifiPortal.onPortalStarted([]() {
    Serial.println("Config Portal started.");
  });

  wifiPortal.onCredentialsSaved([]() {
    Serial.println("New Wi-Fi credentials saved.");
  });

  wifiPortal.onConnected([]() {
    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
  });

  if (!wifiPortal.connectSaved(CONNECT_TIMEOUT_MS)) {
    Serial.println("Saved Wi-Fi is unavailable.");
    Serial.println("Type PORTAL and press Enter to configure Wi-Fi.");
  } else {
    Serial.println("Type PORTAL and press Enter to open Config Portal.");
  }
}

void loop() {
  // Required for async Config Portal and library-managed Auto Reconnect.
  wifiPortal.process();

  // Non-blocking Serial command parser.
  processSerial();

  // Detect when an on-demand Portal has ended.
  if (portalWasActive && !wifiPortal.isPortalActive()) {
    portalWasActive = false;

    if (wifiPortal.isConnected()) {
      Serial.print("Config Portal closed. Wi-Fi connected. IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.print("Config Portal closed: ");
      Serial.println(wifiPortal.lastError());
    }

    Serial.println("Type PORTAL and press Enter to open it again.");
  }

  delay(2);
}
