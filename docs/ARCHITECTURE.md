# Architecture

`ESP32WiFiPortal` is intentionally limited to ESP32 and Arduino-ESP32.

## Runtime flow

1. `connectSaved()` reads SSID/password from the Preferences/NVS namespace `ewp_wifi`.
2. The ESP32 attempts STA connection.
3. On demand, `startConfigPortal()` or `startConfigPortalAsync()` switches to `WIFI_AP_STA`.
4. The SoftAP receives the configured address, or `192.168.4.1/24` by default.
5. `DNSServer` resolves all hostnames to the ESP32 SoftAP address.
6. `WebServer` serves the captive portal and captive-probe redirects.
7. `/scan` returns nearby Wi-Fi networks as JSON.
8. `/save` validates credentials and keeps them temporarily in RAM.
9. `process()` starts and monitors the candidate STA connection while the SoftAP
   remains available.
10. Only a successful candidate connection is written to Preferences/NVS. The
    portal then stops while the connected STA interface remains active.

## Core dependencies

All dependencies are included with Espressif's Arduino-ESP32 core:

- `WiFi.h`
- `WebServer.h`
- `DNSServer.h`
- `Preferences.h`

No third-party runtime library is required.

## Portal addressing

`setPortalIP(...)` validates the complete IPv4 network before changing the stored
configuration. The local address and gateway must be usable RFC 1918 host
addresses in the same contiguous subnet. The active portal configuration is
immutable until `stopConfigPortal()` completes, preventing DNS, redirects, and
the SoftAP from disagreeing about the address.

## State and cleanup invariants

- Blocking and asynchronous portals both execute `process()` and therefore share
  one connection and timeout path.
- At most one candidate STA attempt can be pending or active.
- A failed candidate never overwrites credentials in the `ewp_wifi` namespace.
- Connect timeout, portal timeout, explicit stop, restart, and destruction clear
  candidate flags, timestamps, SSID, and password.
- If a candidate attempt has reached `WiFi.begin()`, cleanup calls
  `WiFi.disconnect(false, false)` to prevent background connection while retaining
  stored Wi-Fi data.
- SoftAP shutdown does not disconnect an unrelated, already-connected STA.
- `connectSaved()` stops an active portal before switching to `WIFI_STA`.
- `eraseCredentials(true)` coordinates successful NVS erasure with portal cleanup
  and Wi-Fi disconnection, so server and Wi-Fi state cannot diverge.

## Design boundaries

The library does not manage application retry queues, cloud uploads, LoRa, OLED, or device business logic. Keep those in the application layer.
