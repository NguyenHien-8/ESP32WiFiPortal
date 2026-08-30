# Architecture

`ESP32WiFiPortal` is intentionally limited to ESP32 + Arduino Core and UI/UX.

## Runtime flow

1. `connectSaved()` reads SSID/password from the Preferences/NVS namespace `ewp_wifi`.
2. The ESP32 attempts STA connection.
3. On demand, `startConfigPortal()` or `startConfigPortalAsync()` switches to `WIFI_AP_STA`.
4. `DNSServer` resolves all hostnames to the ESP32 SoftAP address.
5. `WebServer` serves the captive portal and captive-probe redirects.
6. `/scan` returns nearby Wi-Fi networks as JSON.
7. `/save` validates and stores credentials in NVS.
8. The portal shuts down and the ESP32 attempts the selected STA connection.

## Core dependencies

All dependencies are included with Espressif's Arduino-ESP32 core:

- `WiFi.h`
- `WebServer.h`
- `DNSServer.h`
- `Preferences.h`
- `esp_wifi.h`

No third-party runtime library is required.

## Design boundaries

The library does not manage application retry queues, cloud uploads, LoRa, OLED, or device business logic. Keep those in the application layer.
