# Architecture

`ESP32WiFiPortal` is intentionally limited to ESP32 and Arduino-ESP32.

## Runtime flow

1. `connectSaved()` reads SSID/password from the Preferences/NVS namespace `ewp_wifi`.
2. The library applies DHCP or the validated static STA IP/DNS configuration.
3. The ESP32 attempts the STA connection and `WiFi.onEvent()` reports link/IP
   changes through atomic event flags.
4. On demand, `startConfigPortal()` or `startConfigPortalAsync()` switches to `WIFI_AP_STA`.
5. The SoftAP receives the configured address, or `192.168.4.1/24` by default.
6. `DNSServer` resolves all hostnames to the ESP32 SoftAP address.
7. `WebServer` serves the captive portal and captive-probe redirects.
8. `/scan` returns nearby Wi-Fi networks as JSON.
9. `/save` validates credentials and keeps them temporarily in RAM.
10. `process()` starts and monitors the candidate STA connection while the SoftAP
   remains available.
11. Only a successful candidate connection is written to Preferences/NVS. The
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

## STA addressing

STA and SoftAP addressing are stored separately. `setSTAStaticIP(...)` accepts a
usable local host, a distinct gateway in the same contiguous subnet, and up to
two unicast DNS servers. It only updates the library configuration; the values
are applied with `WiFi.config(...)` immediately before the next managed
`WiFi.begin()`. `useSTADHCP()` selects a zero-address `WiFi.config(...)` call,
which restarts the Arduino-ESP32 DHCP client without altering the SoftAP.

## Wi-Fi events and reconnect ownership

The event handler is installed lazily, avoiding static-initialization ordering
problems for globally declared portal objects. The Arduino event task may set
only atomic bits and the latest disconnect reason. `process()` drains those bits
and performs all state transitions, logs, callbacks, retries, and reconnects in
the application context.

Arduino-ESP32's native Auto Reconnect is disabled after handler registration.
This leaves exactly one owner for connection timing:

- blocking saved connections use the same attempt setup and finite retry policy;
- Portal candidates retry non-blockingly and never write NVS before success;
- normal-operation reconnects run in finite bursts with exponential backoff;
- after a transient burst, a capped cooldown permits recovery from a long router
  outage without a tight retry loop;
- authentication/handshake reasons are terminal and stop automatic retry;
- voluntary `ASSOC_LEAVE` events caused by cleanup never schedule a reconnect.

All elapsed-time tests use unsigned `millis()` subtraction and remain safe across
timer overflow.

## State and cleanup invariants

- Blocking, Portal candidate, and Auto Reconnect attempts share one setup/cancel
  path around `WiFi.config()`, `WiFi.begin()`, and `WiFi.disconnect()`.
- At most one library-managed STA attempt can be active.
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
