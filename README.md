# ESP32WiFiPortal

ESP32-only, Wi-Fi provisioning library for Arduino-ESP32.

## Features

- ESP32 only
- AP mode + DNS captive redirect
- Wi-Fi network scanning
- SSID/password storage in ESP32 Preferences/NVS
- Blocking and non-blocking portal modes
- On-demand configuration support
- No third-party runtime dependencies

## Minimal example

```cpp
#include <ESP32WiFiPortal.h>

ESP32WiFiPortal portal;

void setup() {
  Serial.begin(115200);

  if (!portal.autoConnect("ESP32-Setup", "12345678")) {
    Serial.println(portal.lastError());
    return;
  }

  Serial.println(WiFi.localIP());
}

void loop() {}
```

## Recommended production flow

For deployed devices, do **not** automatically open AP mode every time the router is temporarily unavailable. Prefer:

1. Boot and attempt the saved Wi-Fi credentials.
2. If connection fails, remain offline and retry according to application policy.
3. Enter configuration AP mode only after an explicit button hold or other local user action.
4. Save new credentials, stop the portal, and reconnect.

See `examples/OnDemand`.

## API summary

```cpp
bool connectSaved(uint32_t timeoutMs = 15000);
bool autoConnect(const char* apSSID, const char* apPassword = nullptr,
                 uint32_t connectTimeoutMs = 15000,
                 uint32_t portalTimeoutMs = 0);
bool startConfigPortal(const char* apSSID = "ESP32-Setup",
                       const char* apPassword = nullptr,
                       uint32_t portalTimeoutMs = 0);
bool startConfigPortalAsync(const char* apSSID = "ESP32-Setup",
                            const char* apPassword = nullptr,
                            uint32_t portalTimeoutMs = 0);
void process();
void stopConfigPortal();
bool eraseCredentials(bool disconnect = true);
```

## Security notes

- Use an AP password of at least 8 characters in production.
- Wi-Fi credentials are stored in ESP32 NVS. They are not application-level encrypted by this library.
- The captive portal is HTTP on the local setup AP. Do not expose it to an untrusted network.

## Repository layout

```text
ESP32WiFiPortal/
├── src/
│   ├── ESP32WiFiPortal.h
│   ├── ESP32WiFiPortal.cpp
│   └── PortalPage.h
├── examples/
│   ├── Basic/
│   ├── OnDemand/
│   └── NonBlocking/
├── docs/
│   └── ARCHITECTURE.md
├── library.properties
├── library.json
├── keywords.txt
├── LICENSE
└── README.md
```

## License

Apache License V2.0
