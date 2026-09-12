#pragma once

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

void resetFakeRuntime(bool preservePreferences = false);
void emitWiFiEvent(arduino_event_id_t event, uint8_t reason = 0);
