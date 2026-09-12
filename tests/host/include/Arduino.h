#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#define PROGMEM

class __FlashStringHelper;

#define F(value) reinterpret_cast<const __FlashStringHelper*>(value)

class String {
public:
  String() = default;
  String(const char* value) : _value(value ? value : "") {}
  String(const __FlashStringHelper* value)
      : _value(reinterpret_cast<const char*>(value)) {}

  size_t length() const { return _value.length(); }
  const char* c_str() const { return _value.c_str(); }
  char operator[](size_t index) const { return _value[index]; }
  void reserve(size_t capacity) { _value.reserve(capacity); }
  void remove(size_t index) { _value.erase(index); }

  void trim() {
    const std::string whitespace = " \t\r\n";
    const size_t first = _value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
      _value.clear();
      return;
    }
    const size_t last = _value.find_last_not_of(whitespace);
    _value = _value.substr(first, last - first + 1);
  }

  String& operator=(const char* value) {
    _value = value ? value : "";
    return *this;
  }

  String& operator=(const __FlashStringHelper* value) {
    _value = reinterpret_cast<const char*>(value);
    return *this;
  }

  String& operator+=(const String& value) {
    _value += value._value;
    return *this;
  }

  String& operator+=(const char* value) {
    if (value) _value += value;
    return *this;
  }

  String& operator+=(const __FlashStringHelper* value) {
    _value += reinterpret_cast<const char*>(value);
    return *this;
  }

  String& operator+=(char value) {
    _value += value;
    return *this;
  }

  String& operator+=(int value) {
    _value += std::to_string(value);
    return *this;
  }

  String& operator+=(unsigned int value) {
    _value += std::to_string(value);
    return *this;
  }

  bool operator==(const String& other) const { return _value == other._value; }
  bool operator!=(const String& other) const { return !(*this == other); }

private:
  std::string _value;
};

class IPAddress {
public:
  IPAddress() : _bytes{0, 0, 0, 0} {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : _bytes{a, b, c, d} {}

  uint8_t operator[](size_t index) const { return _bytes[index]; }
  bool operator==(const IPAddress& other) const {
    return _bytes[0] == other._bytes[0] && _bytes[1] == other._bytes[1] &&
           _bytes[2] == other._bytes[2] && _bytes[3] == other._bytes[3];
  }
  bool operator!=(const IPAddress& other) const { return !(*this == other); }

private:
  uint8_t _bytes[4];
};

class FakeSerialClass {
public:
  template <typename T>
  void print(const T&) {}

  template <typename T>
  void println(const T&) {}

  void println() {}
};

struct FakeESPState {
  uint64_t efuseMac = 0x0123456789ABCDEFULL;
  uint32_t cpuMHz = 240;
  uint32_t flashSize = 4U * 1024U * 1024U;
  uint32_t flashSpeed = 80U * 1000U * 1000U;
  uint32_t freeHeap = 128U * 1024U;
  uint32_t restartCalls = 0;
};

extern FakeESPState FakeESP;

class ESPClass {
public:
  uint64_t getEfuseMac() const { return FakeESP.efuseMac; }
  uint32_t getCpuFreqMHz() const { return FakeESP.cpuMHz; }
  uint32_t getFlashChipSize() const { return FakeESP.flashSize; }
  uint32_t getFlashChipSpeed() const { return FakeESP.flashSpeed; }
  uint32_t getFreeHeap() const { return FakeESP.freeHeap; }
  void restart() { ++FakeESP.restartCalls; }
};

extern FakeSerialClass Serial;
extern ESPClass ESP;
extern uint32_t FakeMillis;

uint32_t millis();
void delay(uint32_t milliseconds);
void yield();
