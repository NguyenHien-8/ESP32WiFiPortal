# Architecture

`ESP32WiFiPortal` is intentionally limited to ESP32 and Arduino-ESP32.

## Runtime flow

1. `connectSaved()` validates the single `cred_blob` record in the
   Preferences/NVS namespace `ewp_wifi`, migrating a complete legacy
   `ssid`/`pass` pair when needed.
2. The library applies DHCP or the validated static STA IP/DNS configuration.
3. The ESP32 attempts the STA connection and `WiFi.onEvent()` reports link/IP
   changes through atomic event flags.
4. On demand, `startConfigPortal()` or `startConfigPortalAsync()` switches to `WIFI_AP_STA`.
5. The SoftAP receives the configured address, or `192.168.4.1/24` by default.
6. `DNSServer` resolves all hostnames to the ESP32 SoftAP address.
7. `WebServer` serves the captive portal and captive-probe redirects.
8. `/scan` starts an asynchronous Wi-Fi scan, returns HTTP `202` while it is
   running, then returns the nearby networks as JSON when polling observes it
   ready.
9. `/save` validates exact credential byte lengths without trimming the SSID and
   keeps the values temporarily in RAM.
10. `process()` starts and monitors the candidate STA connection while the SoftAP
   remains available.
11. Only a successful candidate connection is serialized and written with one
    NVS `putBytes()`. Exact read-back plus CRC validation must pass before the
    RAM cache changes. The portal then stops while the connected STA interface
    remains active.

## Credential record and migration

The credential format is explicitly serialized rather than relying on compiler
struct packing. Its 112-byte little-endian layout is:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `EWPC` (`0x43505745`) |
| 4 | 2 | Format version (`1`) |
| 6 | 2 | Record size (`112`) |
| 8 | 1 | SSID byte length (`1..32`) |
| 9 | 1 | Password byte length (`0` or `8..63`) |
| 10 | 33 | Zero-padded SSID storage |
| 43 | 65 | Zero-padded password storage |
| 108 | 4 | CRC32 over bytes `0..107` |

CRC32 uses the IEEE reflected polynomial `0xEDB88320`, initial/final XOR
`0xFFFFFFFF`; the standard `123456789` vector evaluates to `0xCBF43926`.
Metadata, lengths, padding, payload, exact NVS byte count, and CRC all have to
validate. CRC is accidental-corruption detection only; it does not authenticate
or encrypt credentials.

The loader distinguishes four usable outcomes: valid, not found, temporarily
unavailable NVS, and corrupt. A corrupt record is not cached, is not handed to
`WiFi.begin()`, and does not enter the reconnect loop. Temporary NVS open/write
failure is retried only after the normal reconnect cooldown.

Migration prefers a valid blob. If no valid blob exists but both legacy keys
form a valid pair, it writes and reads back the blob before deleting either
legacy key. Thus a reset during migration leaves either the old complete pair,
the new verified blob, or a detectable invalid partial write—never a newly
constructed mixed SSID/password pair. A valid blob plus stale legacy keys is
accepted and the interrupted cleanup is completed opportunistically.

## Core dependencies

All dependencies are included with Espressif's Arduino-ESP32 core:

- `WiFi.h`
- `WebServer.h`
- `DNSServer.h`
- `Preferences.h`
- `esp_wifi.h` (only to stop an in-progress asynchronous scan during cleanup)

No third-party runtime library is required.

## Portal addressing

`setPortalIP(...)` validates the complete IPv4 network before changing the stored
configuration. The local address and gateway may be any usable unicast IPv4 host
in the legacy Class A/B/C ranges; they are no longer restricted to RFC 1918.
Zero, `0.x.x.x`, loopback, multicast/reserved, network, and broadcast addresses
are rejected, and both hosts must be in the same contiguous subnet. Equal local
and gateway addresses remain valid for the SoftAP one-argument API.

Portal masks are limited to `/24` through `/28`, matching the DHCP constraint in
the supported Arduino-ESP32 cores (verified against the `/24.../28` guard in
Arduino-ESP32 3.3.11). Validation also mirrors that core's default inclusive
DHCP pool placement, rejecting a Portal IP or gateway that would fall inside the
pool and make `softAPConfig()` fail later. Class A/B/C does not select the
subnet: the one-argument overload remains `/24`, and the explicit overload uses
CIDR mask validation rather than classful inference. Private RFC 1918 addresses
remain the deployment recommendation to avoid collisions with Internet routes;
`200.5.29.8/24` is supported locally but is not the default.

The address, netmask, subnet, network, broadcast, Portal-policy, and STA-policy
validators are private static inline members of `ESP32WiFiPortal` in the core
header. There is one implementation used by both `setPortalIP(...)` and Portal
startup, no global mutable validation state, and no validation-time heap or
`String` allocation. Because the in-class definitions are implicitly inline,
including `ESP32WiFiPortal.h` from multiple translation units cannot create
duplicate linker symbols. Portal-specific prefix limits remain separate from
the less restrictive static STA policy.

Portal startup validates the stored configuration again immediately before
calling `softAPConfig()`. After SoftAP startup, the library compares `WiFi.softAPIP()`
and `WiFi.softAPSubnetMask()` with the requested values before binding DNS and
HTTP. Any failure or mismatch stops DNS/WebServer/scan state and the SoftAP,
clears Portal runtime buffers, restores a coherent state, and schedules saved
credential recovery through the existing reconnect owner. The active Portal
configuration is immutable until `stopConfigPortal()` completes, preventing
DNS, redirects, and the SoftAP from disagreeing about the address.

## STA addressing

STA and SoftAP addressing are stored separately. `setSTAStaticIP(...)` accepts a
usable local host, a distinct gateway in the same contiguous subnet, and up to
two unicast DNS servers. It only updates the library configuration; the values
are applied with `WiFi.config(...)` immediately before the next managed
`WiFi.begin()`. `useSTADHCP()` selects a zero-address `WiFi.config(...)` call,
which restarts the Arduino-ESP32 DHCP client without altering the SoftAP.

For runtime Portal and Auto Reconnect attempts, setup is split into cooperative
phases. One call disconnects STA when cleanup is needed, later calls observe the
20 ms settle interval with unsigned `millis()` subtraction, and a ready call
applies the STA configuration and invokes `WiFi.begin()`. A retry following
`cancelSTAConnection()` reuses the already-clean STA state and does not issue a
second redundant disconnect. Blocking startup APIs drive these same phases in
their compatibility loops.

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
- authentication/handshake failures for saved credentials remain recoverable,
  because those reason codes are ambiguous under weak signal or router restart;
- clear authentication rejection can still terminate an unproven Portal or
  initial blocking candidate without changing the credentials in NVS;
- voluntary `ASSOC_LEAVE` events caused by cleanup never schedule a reconnect.

All elapsed-time tests use unsigned `millis()` subtraction and remain safe across
timer overflow.

`setConnectTimeout(0)` and `connectSaved(0)` are normalized to 15000 ms. Portal
candidates, blocking saved connections, and every Auto Reconnect attempt
therefore have a finite deadline; `process()` contains no wait loop and advances
at most the currently ready state transition.

## State and cleanup invariants

- Blocking, Portal candidate, and Auto Reconnect attempts share one setup/cancel
  path around `WiFi.config()`, `WiFi.begin()`, and `WiFi.disconnect()`.
- At most one library-managed STA attempt can be active.
- A candidate that fails to connect never starts an NVS credential write.
- A failed/short blob write never updates the RAM cache and remains detectable
  by exact record-size and CRC checks on reboot.
- Credentials are read into a synchronized object cache once and reused by
  reconnect attempts; successful saves and erases update that cache.
- Closing or timing out an unsuccessful Portal schedules the saved credentials
  through the normal non-blocking reconnect cooldown, without reopening Portal.
- Connect timeout, portal timeout, explicit stop, restart, and destruction clear
  candidate flags, timestamps, SSID, and password.
- If a candidate attempt has reached `WiFi.begin()`, cleanup calls
  `WiFi.disconnect(false, false)` to prevent background connection while retaining
  stored Wi-Fi data.
- SoftAP shutdown does not disconnect an unrelated, already-connected STA.
- `connectSaved()` stops an active portal before switching to `WIFI_STA`.
- `eraseCredentials(true)` coordinates successful NVS erasure with portal cleanup
  and Wi-Fi disconnection, so server and Wi-Fi state cannot diverge.
- `eraseCredentials()` removes `cred_blob`, `ssid`, and `pass` explicitly while
  leaving unrelated namespace values untouched.

## Heap behavior on repeated paths

Saved SSID/password values are cached after the first Preferences read, so each
Auto Reconnect attempt reuses the same buffers instead of reopening NVS and
constructing short-lived `String` objects. Candidate buffers retain their small
credential-sized capacity between Portal submissions and are synchronized with
the cache only after a successful connection and NVS commit.

Portal scan/status/properties JSON shares one reserved response buffer while the
Portal is active. Properties uses fixed-size MAC buffers and is generated only
when requested, so opening the view repeatedly does not create a background
polling workload. Scan duplicate detection keeps compact SSID hashes and performs an exact
SSID comparison on a hash match, avoiding the previous repeated `WiFi.SSID()`
allocations for every earlier scan result. The driver scan itself is asynchronous;
`process()` only polls its state, while the browser polls `/scan` after HTTP `202`.
`WiFi.scanDelete()` still runs before the completed response is sent, and an
active scan is stopped and cleaned if the Portal closes. All scan-only capacity
is released when the Portal stops.

## Design boundaries

The library does not manage application retry queues, cloud uploads, LoRa, OLED, or device business logic. Keep those in the application layer.
