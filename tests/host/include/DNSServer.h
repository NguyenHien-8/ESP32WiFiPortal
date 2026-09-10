#pragma once

#include <Arduino.h>

enum class DNSReplyCode : uint8_t { NoError };

struct FakeDNSServerState {
  bool startResult = true;
  bool active = false;
  uint32_t startCalls = 0;
  uint32_t stopCalls = 0;
  String domain;
  IPAddress address;
};

extern FakeDNSServerState FakeDNS;

class DNSServer {
public:
  void setErrorReplyCode(DNSReplyCode) {}

  bool start(uint16_t, const char* domain, const IPAddress& address) {
    ++FakeDNS.startCalls;
    FakeDNS.domain = domain;
    FakeDNS.address = address;
    FakeDNS.active = FakeDNS.startResult;
    return FakeDNS.startResult;
  }

  void stop() {
    ++FakeDNS.stopCalls;
    FakeDNS.active = false;
  }

  void processNextRequest() {}
};
