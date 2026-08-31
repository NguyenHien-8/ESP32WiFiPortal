# ESP32WiFiPortal 1.1.1

ESP32-only Wi-Fi provisioning library for Arduino-ESP32.

## Features

- SoftAP captive portal with DNS redirection
- Private default portal address: `192.168.4.1`
- Configurable portal address through `setPortalIP(...)`
- Wi-Fi scanning and Preferences/NVS credential storage
- Blocking, non-blocking, and on-demand portal modes
- Safe cancellation of pending STA attempts when the portal stops or times out
- No third-party runtime dependency

## Minimal example

```cpp
#include <ESP32WiFiPortal.h>

ESP32WiFiPortal portal;

void setup() {
  Serial.begin(115200);

  portal.onPortalStarted([]() {
    Serial.print("Open http://");
    Serial.println(portal.portalIP());
  });

  if (!portal.autoConnect("ESP32-Setup", "12345678")) {
    Serial.println(portal.lastError());
  }
}

void loop() {}
```

The default portal address is `192.168.4.1`. `portalIP()` also returns the
configured address before the portal starts.

## Custom portal IP

Configure the portal before starting it:

```cpp
if (!portal.setPortalIP(IPAddress(192, 168, 50, 1))) {
  Serial.println(portal.lastError());
}
```

The one-argument overload uses the local IP as gateway and a
`255.255.255.0` subnet. An explicit network can also be supplied:

```cpp
portal.setPortalIP(
    IPAddress(10, 10, 0, 1),
    IPAddress(10, 10, 0, 1),
    IPAddress(255, 255, 255, 0));
```

Only usable RFC 1918 host addresses are accepted. The address and gateway must
belong to the same valid subnet. The configuration cannot be changed while the
portal is active.

## Portal timeout and stop behavior

Both blocking and asynchronous modes use the same state machine. When a portal
timeout or `stopConfigPortal()` occurs during a new STA connection attempt, the
library disconnects that attempt, clears pending credentials from RAM, stops the
HTTP server and DNS, and shuts down only the SoftAP interface. Stored credentials
are not modified until a new STA connection succeeds.

If the ESP32 was already connected before opening the portal and no replacement
attempt is running, stopping the portal leaves that STA connection intact. A
timed-out blocking portal returns `false` when no STA connection remains; async
code can inspect `state()` and `lastError()` after `process()`.

For deployed devices, prefer an explicit button or other local action before
opening configuration mode. See `examples/OnDemand`.

## API summary

```cpp
bool connectSaved(uint32_t timeoutMs = 15000);
bool autoConnect(const char* apSSID = "ESP32-Setup",
                 const char* apPassword = nullptr,
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

bool setPortalIP(const IPAddress& localIP);
bool setPortalIP(const IPAddress& localIP,
                 const IPAddress& gateway,
                 const IPAddress& subnet);

bool eraseCredentials(bool disconnect = true);
```

## Compatibility notes for 1.1.1

- Existing public APIs remain source-compatible.
- The default SoftAP address changed from `200.5.29.8` to `192.168.4.1`.
- `setPortalIP(...)` is additive; existing sketches need no source changes.
- The captive portal remains HTTP on the isolated setup AP. Use a strong AP
  password and do not expose the setup network to untrusted clients.

## Repository layout

```text
ESP32WiFiPortal/
├── src/
├── examples/
│   ├── Basic/
│   ├── CustomIP/
│   ├── NonBlocking/
│   └── OnDemand/
├── docs/
│   ├── ARCHITECTURE.md
│   └── FlowChart.md
├── library.properties
└── library.json
```

## License

Apache License V2.0
