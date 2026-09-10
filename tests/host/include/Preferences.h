#pragma once

#include <Arduino.h>

class Preferences {
public:
  bool begin(const char*, bool) { return true; }
  void end() {}
  String getString(const char*, const char* fallback) { return String(fallback); }
  size_t putString(const char*, const String& value) { return value.length(); }
  bool clear() { return true; }
};
