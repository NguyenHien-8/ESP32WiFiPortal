#pragma once

#include <Arduino.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

struct FakePreferencesState {
  bool beginResult = true;
  bool removeResult = true;
  bool opened = false;
  bool readOnly = true;
  size_t putBytesLimit = std::numeric_limits<size_t>::max();
  std::vector<size_t> putBytesLimits;
  size_t getBytesLimit = std::numeric_limits<size_t>::max();
  uint32_t beginCalls = 0;
  uint32_t putBytesCalls = 0;
  uint32_t removeCalls = 0;
  std::map<std::string, std::vector<uint8_t>> bytes;
  std::map<std::string, std::string> strings;
};

extern FakePreferencesState FakePreferences;

class Preferences {
public:
  bool begin(const char*, bool readOnly) {
    ++FakePreferences.beginCalls;
    if (!FakePreferences.beginResult) return false;
    FakePreferences.opened = true;
    FakePreferences.readOnly = readOnly;
    return true;
  }

  void end() { FakePreferences.opened = false; }

  bool isKey(const char* key) const {
    const std::string name(key ? key : "");
    return FakePreferences.bytes.count(name) != 0 ||
           FakePreferences.strings.count(name) != 0;
  }

  String getString(const char* key, const char* fallback) const {
    const auto found = FakePreferences.strings.find(key ? key : "");
    return found == FakePreferences.strings.end()
               ? String(fallback)
               : String(found->second.c_str());
  }

  size_t putString(const char* key, const String& value) {
    if (!FakePreferences.opened || FakePreferences.readOnly) return 0;
    FakePreferences.strings[key ? key : ""] = value.c_str();
    return value.length();
  }

  size_t getBytesLength(const char* key) const {
    const auto found = FakePreferences.bytes.find(key ? key : "");
    return found == FakePreferences.bytes.end() ? 0 : found->second.size();
  }

  size_t getBytes(const char* key, void* buffer, size_t maxLength) const {
    const auto found = FakePreferences.bytes.find(key ? key : "");
    if (found == FakePreferences.bytes.end() || !buffer) return 0;
    const size_t copied = std::min(
        std::min(found->second.size(), maxLength), FakePreferences.getBytesLimit);
    if (copied > 0) memcpy(buffer, found->second.data(), copied);
    return copied;
  }

  size_t putBytes(const char* key, const void* value, size_t length) {
    ++FakePreferences.putBytesCalls;
    if (!FakePreferences.opened || FakePreferences.readOnly || !value) return 0;
    size_t limit = FakePreferences.putBytesLimit;
    if (!FakePreferences.putBytesLimits.empty()) {
      limit = FakePreferences.putBytesLimits.front();
      FakePreferences.putBytesLimits.erase(
          FakePreferences.putBytesLimits.begin());
    }
    const size_t written = std::min(length, limit);
    const uint8_t* bytes = static_cast<const uint8_t*>(value);
    FakePreferences.bytes[key ? key : ""] =
        std::vector<uint8_t>(bytes, bytes + written);
    return written;
  }

  bool remove(const char* key) {
    ++FakePreferences.removeCalls;
    if (!FakePreferences.opened || FakePreferences.readOnly ||
        !FakePreferences.removeResult) {
      return false;
    }
    const std::string name(key ? key : "");
    FakePreferences.bytes.erase(name);
    FakePreferences.strings.erase(name);
    return true;
  }

  bool clear() {
    if (!FakePreferences.opened || FakePreferences.readOnly) return false;
    FakePreferences.bytes.clear();
    FakePreferences.strings.clear();
    return true;
  }
};
