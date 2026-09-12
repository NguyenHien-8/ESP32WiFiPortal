# ESP32WiFiPortal 2.1.2 Operational Flowcharts

This document describes the state machine of `ESP32WiFiPortal` 2.1.2 for blocking connections, blocking/non-blocking Config Portal operation, Wi-Fi events, retries, Auto Reconnect, and asynchronous Wi-Fi scanning.

## STA Address Configuration

```mermaid
flowchart TD
    A[Application selects STA configuration] --> B{Call setSTAStaticIP?}
    B -- No --> C[Default DHCP]
    B -- Yes --> D{Are IP and Gateway different hosts in the same subnet?}
    D -- No --> E[Return false and keep previous configuration]
    D -- Yes --> F{Valid DNS?}
    F -- No --> E
    F -- Yes --> G[Store Static IP and DNS in the object]
    C --> H[Before WiFi.begin, call WiFi.config with zero addresses]
    G --> I[Before WiFi.begin, call WiFi.config with Static IP and DNS]
    H --> J[Configure STA only]
    I --> J
```

`useSTADHCP()` selects DHCP again for the next library-managed connection. The
STA configuration does not read or modify the SoftAP Portal IP.

## Startup and Connection Using Saved Credentials

```mermaid
flowchart TD
    A[Application starts] --> B{Was setPortalIP called?}
    B -- No --> C[Use default 192.168.4.1/24]
    B -- Yes --> D{Are IP and gateway unicast hosts in the same /24 to /28 subnet?}
    D -- No --> E[Return false and set lastError]
    D -- Yes --> F[Store Portal IP configuration in the object]
    C --> G[connectSaved or autoConnect]
    F --> G
    G --> H{Valid cred_blob?}
    H -- Yes --> J[Apply DHCP or Static STA IP and start connection]
    H -- Missing or corrupt --> HB{Valid cred_backup?}
    HB -- Yes --> HC[Use old backup and repair primary opportunistically]
    HC --> J
    HB -- No --> R{Valid legacy ssid/pass?}
    R -- Yes --> S[Write one blob, read it back, and verify CRC]
    S --> T{Verification successful?}
    T -- Yes --> U[Delete the two legacy keys]
    U --> J
    T -- No --> I{Using autoConnect?}
    R -- No --> I
    J --> K{Connected before timeout?}
    K -- Yes --> L[State = Connected]
    K -- No --> M[WiFi.disconnect, return false, and schedule saved credentials]
    I -- Yes --> N[Open blocking Config Portal]
    I -- No --> O[Return false to application]
    M --> I
```

`connectSaved()` reads the `ewp_wifi` namespace; connection failure does not
delete saved credentials. A corrupted record is rejected and is not used by
Auto Reconnect; a temporary NVS-open failure is retried after cooldown. If a
Portal is active, the library stops and cleans up the Portal before switching
to `WIFI_STA`. When Auto Reconnect is enabled, a failed blocking attempt still
schedules a non-blocking retry; `autoConnect()` cancels that schedule when it
immediately switches to the Portal, so there are no two connection owners.

## Config Portal Initialization

```mermaid
flowchart TD
    A[startConfigPortal or startConfigPortalAsync] --> V{Revalidate Portal IP gateway subnet}
    V -- Invalid --> Y[Return false without starting resources]
    V -- Valid --> B[stopConfigPortal to clean up the previous session]
    B --> C[Switch to WIFI_AP_STA]
    C --> D[Apply local IP, gateway, and subnet using softAPConfig]
    D --> E{IP configuration successful?}
    E -- No --> X[Cleanup and restore old credentials if STA is offline]
    E -- Yes --> F[Start SoftAP]
    F --> G{SoftAP started successfully?}
    G -- No --> X
    G -- Yes --> H[Read back runtime SoftAP IP and subnet]
    H --> I{Matches requested configuration?}
    I -- No --> X
    I -- Yes --> J[Start WebServer on port 80]
    J --> K[Start wildcard DNS on port 53]
    K --> L{DNS started successfully?}
    L -- No --> X
    L -- Yes --> M[State = Portal and call onPortalStarted]
```

The Portal IP remains fixed throughout the active session. `setPortalIP()`
returns `false` if it is called while the Portal is active, ensuring that
SoftAP, DNS, and HTTP redirects always use the same address.

The validator accepts valid unicast Class A/B/C addresses but uses CIDR `/24`
through `/28` for SoftAP DHCP; it does not infer classful `/8` or `/16` masks.
RFC 1918 addresses are still recommended to avoid routing conflicts.
`200.5.29.8/24` is supported for the local SoftAP, but it does not replace the
safe default `192.168.4.1/24`.

The validator also calculates the default lease pool used by Arduino-ESP32
3.3.11 in advance and rejects an IP/gateway inside that pool. This prevents a
case where the setter succeeds but `softAPConfig()` would inevitably fail at
startup.

Pure IPv4 validation checks are packaged as `private static inline` helpers
directly inside the `ESP32WiFiPortal` class. Therefore, the library has only
one implementation, does not expand the public API, allocates no heap memory
during validation, and does not cause multiple-definition errors when multiple
translation units include the main header.

## Non-Blocking Wi-Fi Scan in the Portal

```mermaid
flowchart TD
    A[GET /scan] --> B{Scan state}
    B -- Idle --> C[Start scan async]
    C --> D[HTTP 202 scanning]
    B -- Scanning --> E{scanComplete}
    E -- Running --> D
    E -- Ready --> F[State = Ready]
    E -- Failed or timeout --> G[HTTP 503 and cleanup]
    D --> H[Browser waits 400 ms, then polls again]
    H --> A
    B -- Ready --> I[Build JSON from results]
    F --> I
    I --> J[scanDelete, HTTP 200]
```

The scan driver runs asynchronously, so each `process()` call only polls the
state and then continues serving DNS, HTTP, and the state machine. Stopping the
Portal or receiving a valid connection request cancels any running scan and
releases its results; two scans never run concurrently.

## Receiving and Testing New Credentials

```mermaid
flowchart TD
    A[POST /save] --> B{SSID is 1-32 bytes and password is empty, 8-63 bytes, or 64 hex digits?}
    B -- No --> C[HTTP 400]
    B -- Yes --> D{Pending or active attempt already exists?}
    D -- Yes --> E[HTTP 409]
    D -- No --> F[Temporarily keep new credentials in RAM]
    F --> G[Return Connecting page]
    G --> H[Disconnect STA phase if needed]
    H --> U[Settling phase using millis]
    U --> V[Apply STA config phase]
    V --> W[WiFi.begin phase]
    W --> I{Is STA connected?}
    I -- No --> J{Connect timeout exceeded?}
    J -- No --> I
    J -- Yes --> K{New credentials explicitly rejected?}
    K -- Yes --> L[Cancel attempt, do not save to NVS, keep Portal]
    K -- No --> R{Configured retries remaining?}
    R -- Yes --> S[Schedule retry using millis and backoff]
    S --> H
    R -- No --> T[Clear temporary credentials and keep Portal]
    I -- Yes --> M[Verify backup of old record, then write and verify cred_blob]
    M --> N{Read back full bytes with valid metadata and CRC?}
    N -- No --> O[Disconnect candidate STA, clear temporary data, keep Portal]
    N -- Yes --> P[Call callback, stop Portal, keep STA connected]
    P --> Q[State = Connected]
```

The old credentials in NVS are not modified when the candidate cannot connect
or when the Portal times out. Writing occurs only after `WL_CONNECTED`. A
handshake timeout does not automatically prove that the password is incorrect,
because weak signal conditions or an AP restart may also cause it.

SSIDs from both the scan list and **Advanced Wi-Fi Setting** are preserved byte
for byte without calling `trim()`. The Advanced view uses the same POST `/save`,
the same candidate RAM storage, and the same state machine; the password is not
placed in the URL or browser storage.

## Wi-Fi Events and Auto Reconnect

```mermaid
flowchart TD
    A[Arduino Wi-Fi event task] --> B{Which event?}
    B -- STA Connected --> C[Set atomic Connected bit]
    B -- Got IP --> D[Set atomic Got-IP bit]
    B -- Disconnected --> E[Store reason and set atomic Disconnect bit]
    C --> F[Callback returns immediately]
    D --> F
    E --> F
    G[Application calls process] --> H[Atomic exchange to consume event]
    H --> I[Short log and update state]
    I --> J{Disconnect during normal operation?}
    J -- No --> K[Let the current owner handle the attempt]
    J -- Yes --> N[Schedule Reconnect after retry interval]
    N --> S[Disconnect if needed, Settling, Config, WiFi.begin]
    S --> O[Finite attempts with exponential backoff]
    O --> P{Connected?}
    P -- Yes --> Q[State = Connected, reset retry]
    P -- No --> R[Cooldown using max retry interval]
    R --> N
```

Arduino-ESP32 Auto Reconnect is disabled when the library's event handler is
installed. Therefore, only this state machine calls `WiFi.begin()` and cancels
attempts. Loss of the AP is treated as temporary, so the device can still
recover after cooldown. With saved credentials, `AUTH_FAIL` and handshake
timeouts also continue through cooldown because these reasons can occur due to
packet loss, AP overload, or router restart. If the password has actually
changed, the cooldown limits the retry frequency and prevents a reconnect
storm. The callback does not call Serial, DNS, WebServer, Preferences, or any
blocking Wi-Fi API. `setConnectTimeout(0)` and `connectSaved(0)` are normalized
to 15000 ms, so blocking connect, Portal candidate, and Auto Reconnect cannot
hold an attempt indefinitely.

## Portal Timeout, Stop, and Restart

```mermaid
flowchart TD
    A[Portal timeout or stop] --> B[Mark Portal inactive]
    B --> C{Did candidate call WiFi.begin?}
    C -- Yes --> D[WiFi.disconnect false, false]
    C -- No --> E[Do not disconnect an independent STA]
    D --> F[Reset pending and active flags]
    E --> F
    F --> G[Clear timestamp, temporary SSID, and password]
    G --> H[Stop WebServer and DNS]
    H --> I[Stop SoftAP]
    I --> J{Is STA still connected?}
    J -- Yes --> K[State = Connected]
    J -- No --> L{Auto Reconnect enabled and old credentials available?}
    L -- Yes --> M[Schedule old credentials, State = Connecting]
    L -- No --> N[Timeout: Failed; explicit stop: Idle]
```

In blocking mode, the loop ends immediately when `_portalActive` becomes
`false`. In non-blocking mode, the application must call `process()`
frequently. Because both modes use the same state machine, timeout and cleanup
behavior is consistent, and no candidate STA continues running in the
background. If valid old credentials are still available in cache/NVS,
`process()` restores them using non-blocking Auto Reconnect without reopening
the Portal automatically. When the Portal restarts, the pending recovery
schedule is canceled before the new Portal session starts, so two owners never
call `WiFi.begin()` concurrently.

## Explicit Credential Management

```mermaid
flowchart LR
    A[eraseCredentials false] --> B[Delete cred_blob, cred_backup, ssid, and pass]
    B --> C[Do not actively disconnect Wi-Fi]
    D[eraseCredentials true] --> F[Delete cred_blob, cred_backup, ssid, and pass]
    F --> E[Stop and clean up Portal if active]
    E --> G[Disconnect Wi-Fi and erase core Wi-Fi configuration]
    G --> H[State = Idle]
```

`eraseCredentials()` is the only deletion operation explicitly requested by
the public API. Connect timeout, Portal timeout, and `stopConfigPortal()` do not
delete saved credentials.

---

## Async Wi-Fi Scan

```mermaid
flowchart TD
    A[Browser GET /scan] --> B[handleScan]
    B --> C{Pending/active connection?}
    C -- Yes --> D[HTTP 409]
    C -- No --> E[processScan]
    E --> F{ScanState}

    F -- Idle --> G[WiFi.scanNetworks true,true]
    G --> H{RUNNING?}
    H -- Yes --> I[State = Scanning]
    H -- No, result >= 0 --> J[State = Ready]
    H -- Error --> K[State = Failed]

    F -- Scanning --> L[WiFi.scanComplete]
    L --> M{RUNNING and < 15 s?}
    M -- Yes --> I
    M -- No --> N{result >= 0?}
    N -- Yes --> J
    N -- No --> K

    I --> O[HTTP 202]
    O --> P[Browser waits 400 ms]
    P --> A

    J --> Q[Build JSON]
    Q --> R[resetScan false / scanDelete]
    R --> S[HTTP 200]

    K --> T[HTTP 503]
    T --> U[resetScan false]
```

While the browser is waiting for 400 ms, `process()` can still call
`processScan()` and transition `Scanning -> Ready/Failed`. Therefore, it should
not be described as if only `/scan` can advance the scan.

## Cooperative Non-Blocking `process()` Function — Accurate Simplified Flow

`process()` **does not wait** for a Wi-Fi connection, settle delay, retry delay,
or network scan. However, a single `process()` call may service multiple tasks
that are already ready (event, scan, DNS, HTTP) before returning; it is not
limited to performing exactly one operation per call.

```mermaid
flowchart TD
    A[loop] --> B[process]
    B --> C[processWiFiEvents]
    C --> D{Portal active?}

    D -- Yes --> E[processScan]
    E --> F[DNS processNextRequest]
    F --> G[WebServer handleClient]
    G --> H{Portal timeout?}
    H -- Yes --> I[stopConfigPortal, update state, return]
    H -- No --> J{Has pending connection reached its scheduled time?}
    J -- Yes --> K[beginPendingConnection, return]
    J -- No --> L{Portal connection attempt active?}
    L -- Yes --> M[advanceSTAConnection / check connected, terminal, timeout]
    M --> N{Need to wait longer?}
    N -- Yes --> O[return]
    N -- No --> P[Handle success/failure/retry, then return or continue]
    L -- No --> Q[processAutoReconnect is a no-op because Portal is active]

    D -- No --> R[processAutoReconnect]
    Q --> S[return]
    R --> S
    P --> S
    O --> S
    S --> T[Application continues running]
    T --> A
```

Example of a Portal candidate connection phase:

```text
process #1
  -> pending delay of 350 ms has elapsed
  -> beginSTAConnection()
  -> if STA is not in a clean state: WiFi.disconnect()
  -> Phase = Settling, settle delay = 20 ms
  -> return

process #2 ... #N
  -> advanceSTAConnection()
  -> if settle delay has not elapsed: return quickly

next process after settle delay has elapsed
  -> applySTAConfig()
  -> WiFi.begin()
  -> Phase = Connecting
  -> return after immediate checks

subsequent process calls
  -> read event / WiFi.status()
  -> check terminal failure or connect timeout using millis()
  -> no busy-wait
```

If STA is already in a disconnected/clean state, `_connectionSettleDelayMs` may
be `0`; therefore, **not every attempt is required to wait 20 ms**.

Note: `process()` is cooperative non-blocking, but the `connectSaved()` and
`startConfigPortal()` APIs are still blocking APIs by design.
`startConfigPortal()` achieves blocking behavior by repeatedly calling
`process()` internally.

## Overall Operational Flow

```mermaid
flowchart TD
    A[ESP32 starts / object is created] --> B[Application selects API]

    B -->|connectSaved| C[ensureCredentialCache]
    B -->|autoConnect| D[connectSaved]
    B -->|startConfigPortal / Async / On-Demand trigger| P[openPortal]

    C --> E{Valid credentials?}
    E -- No --> F[Return false; may schedule recovery for temporary NVS failure]
    E -- Yes --> G[Blocking STA connect using saved credentials]
    G --> H{Success?}
    H -- Yes --> I[State = Connected]
    H -- No --> J{Saved recovery already scheduled?}
    J -- Yes --> AH
    J -- No --> JO[Return false; offline/Failed]

    D --> K{connectSaved success?}
    K -- Yes --> I
    K -- No --> P

    P --> Q[WIFI_AP_STA + SoftAP + DNS + WebServer]
    Q --> R[State = Portal]
    R --> RA{Portal timeout or stop?}
    RA -- Yes --> AE
    RA -- No --> S[Async scan or manual SSID entry]
    S --> T[POST /save]
    T --> U[Keep candidate in RAM, wait 350 ms]
    U --> V[Non-blocking STA candidate: Settling -> Config -> WiFi.begin]
    V --> W{Connected?}

    W -- Success --> X[Verify old backup, then write/read-back cred_blob + CRC]
    X --> Y{Save successful?}
    Y -- Yes --> Z[Update cache, callbacks, stop Portal]
    Z --> I
    Y -- No --> AA[Disconnect candidate, keep Portal]

    W -- Terminal auth fail --> AA
    W -- Timeout --> AB{Portal retries remaining?}
    AB -- Yes --> AC[Backoff using millis, then retry]
    AC --> V
    AB -- No --> AA

    AA --> R
    AE[Cleanup HTTP/DNS/scan/SoftAP]
    AE --> AF{STA still connected?}
    AF -- Yes --> I
    AF -- No --> AG{Auto Reconnect enabled and old credentials valid/readable?}
    AG -- Yes --> AH[Schedule saved recovery, State = Connecting]
    AG -- No --> AI[Timeout -> Failed; explicit stop -> Idle]

    I --> AJ[Wi-Fi event callback only sets atomic bits]
    AJ --> AK[processWiFiEvents]
    AK --> AL{STA lost with no active owner?}
    AL -- No --> I
    AL -- Yes --> AM{Auto Reconnect enabled and Portal inactive?}
    AM -- No --> AN[No automatic reconnect]
    AM -- Yes --> AH

    AH --> AO[processAutoReconnect]
    AO --> AP[WIFI_STA, beginSTAConnection owner Reconnect]
    AP --> AQ{Success before timeout?}
    AQ -- Yes --> I
    AQ -- No --> AR{Retries remaining in burst?}
    AR -- Yes --> AS[Exponential backoff]
    AS --> AO
    AR -- No --> AT[Cooldown = max retry interval]
    AT --> AO
```

### Key Points to Understand Correctly

- The constructor **does not automatically** read NVS, connect STA, or open the Portal; every flow starts from an API called by the application.
- Only `autoConnect()` automatically transitions from a failed `connectSaved()` attempt to a **blocking Config Portal**. Calling `connectSaved()` alone does not open the Portal automatically; for example, an On-Demand setup can keep the device offline until the user explicitly activates the Portal.
- The Config Portal does not require selecting an SSID from scan results: Advanced Wi-Fi Setting can submit a hidden/unlisted SSID through the same `POST /save` endpoint.
- Candidate credentials are written to NVS only after STA has connected. If save/read-back/CRC verification fails, the candidate is disconnected and the Portal remains active.
- When the Portal times out/stops while STA is offline, the library restores the old Wi-Fi connection only if Auto Reconnect is enabled and the old credentials are still valid or can be read again. Otherwise, a timeout ends in `Failed`, while an explicit stop ends in `Idle`.
- Auto Reconnect runs inside `process()` and uses finite retries + exponential backoff + cooldown; Arduino core Auto Reconnect is disabled to prevent two competing reconnect flows.
