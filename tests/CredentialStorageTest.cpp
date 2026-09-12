#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include "host/TestAssert.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using CredentialStatus = ESP32WiFiPortalTestAccess::CredentialStatus;

std::string repeated(char value, size_t count) {
  return std::string(count, value);
}

void seedLegacy(const std::string& ssid, const std::string& password) {
  FakePreferences.strings["ssid"] = ssid;
  FakePreferences.strings["pass"] = password;
}

std::vector<uint8_t> makeRecord(const char* ssid, const char* password) {
  std::vector<uint8_t> record(
      ESP32WiFiPortalTestAccess::credentialRecordSize());
  assert(ESP32WiFiPortalTestAccess::serializeCredentials(
      String(ssid), String(password), record.data(), record.size()));
  return record;
}

void refreshCRC(std::vector<uint8_t>& record) {
  const size_t crcOffset = record.size() - 4;
  const uint32_t crc = ESP32WiFiPortalTestAccess::credentialCRC32(
      record.data(), crcOffset);
  record[crcOffset] = static_cast<uint8_t>(crc);
  record[crcOffset + 1] = static_cast<uint8_t>(crc >> 8);
  record[crcOffset + 2] = static_cast<uint8_t>(crc >> 16);
  record[crcOffset + 3] = static_cast<uint8_t>(crc >> 24);
}

void expectCorruptRecordRejected(const std::vector<uint8_t>& record) {
  resetFakeRuntime();
  FakePreferences.bytes["cred_blob"] = record;
  ESP32WiFiPortal portal;
  portal.setLogging(false);
  assert(!portal.hasSavedCredentials());
  assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
         CredentialStatus::Corrupt);
  portal.setAutoReconnect(true);
  FakeMillis = 1000;
  portal.process();
  assert(FakeWiFi.beginCalls == 0);
  assert(!ESP32WiFiPortalTestAccess::reconnectScheduled(portal));
}

}  // namespace

int main() {
  static const uint8_t kCRCVector[] = "123456789";
  assert(ESP32WiFiPortalTestAccess::credentialCRC32(kCRCVector, 9) ==
         0xCBF43926UL);
  assert(ESP32WiFiPortalTestAccess::credentialRecordSize() == 112);

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    const std::string maxSSID = repeated('S', 32);
    const std::string maxPassword = repeated('P', 63);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String(maxSSID.c_str()), String(maxPassword.c_str())));
    assert(FakePreferences.putBytesCalls == 1);
    assert(FakePreferences.bytes["cred_blob"].size() == 112);
    assert(std::string(portal.savedSSID().c_str()) == maxSSID);
    assert(std::string(
               ESP32WiFiPortalTestAccess::savedPassword(portal).c_str()) ==
           maxPassword);
    const uint32_t writesBeforeNoOp = FakePreferences.putBytesCalls;
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String(maxSSID.c_str()), String(maxPassword.c_str())));
    assert(FakePreferences.putBytesCalls == writesBeforeNoOp);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == repeated('S', 32));
    assert(std::string(
               ESP32WiFiPortalTestAccess::savedPassword(portal).c_str()) ==
           repeated('P', 63));
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Open network"), String("")));
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String(""), String("12345678")));
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String(repeated('S', 33).c_str()), String("12345678")));
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("ssid"), String("1234567")));
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("ssid"), String(repeated('P', 64).c_str())));
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("raw-psk"), String(repeated('A', 64).c_str())));
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("ssid"), String(repeated('A', 65).c_str())));
  }

  const std::vector<uint8_t> valid = makeRecord("Exact SSID ", "password");
  {
    std::vector<uint8_t> damaged = valid;
    damaged[10] ^= 0x40;
    expectCorruptRecordRejected(damaged);
  }
  {
    std::vector<uint8_t> damaged = valid;
    damaged[0] ^= 1;
    refreshCRC(damaged);
    expectCorruptRecordRejected(damaged);
  }
  {
    std::vector<uint8_t> damaged = valid;
    damaged[4] = 2;
    refreshCRC(damaged);
    expectCorruptRecordRejected(damaged);
  }
  {
    std::vector<uint8_t> damaged = valid;
    damaged[6] = 111;
    refreshCRC(damaged);
    expectCorruptRecordRejected(damaged);
  }
  {
    std::vector<uint8_t> damaged = valid;
    damaged[8] = 33;
    refreshCRC(damaged);
    expectCorruptRecordRejected(damaged);
  }
  {
    std::vector<uint8_t> damaged(valid.begin(), valid.end() - 1);
    expectCorruptRecordRejected(damaged);
  }

  resetFakeRuntime();
  {
    FakePreferences.bytes["cred_blob"] = valid;
    FakePreferences.getBytesLimit = valid.size() - 1;
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::Corrupt);
  }

  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::NotFound);
  }
  resetFakeRuntime();
  {
    FakePreferences.beginResult = false;
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::Unavailable);
    FakePreferences.beginResult = true;
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::NotFound);
  }

  resetFakeRuntime();
  {
    seedLegacy("Legacy SSID", "legacy-pass");
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Legacy SSID");
    assert(FakePreferences.putBytesCalls == 1);
    assert(FakePreferences.bytes["cred_blob"].size() == 112);
    assert(FakePreferences.strings.count("ssid") == 0);
    assert(FakePreferences.strings.count("pass") == 0);
  }

  resetFakeRuntime();
  {
    FakePreferences.strings["ssid"] = "Incomplete legacy";
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::Corrupt);
    assert(FakePreferences.putBytesCalls == 0);
  }

  resetFakeRuntime();
  {
    FakePreferences.bytes["cred_blob"] = valid;
    seedLegacy("Stale legacy", "stale-password");
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Exact SSID ");
    assert(FakePreferences.putBytesCalls == 0);
    assert(FakePreferences.strings.empty());
  }

  resetFakeRuntime();
  {
    seedLegacy("Power cut SSID", "power-cut-pass");
    FakePreferences.putBytesLimit = 41;
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::Unavailable);
    assert(FakePreferences.strings.count("ssid") == 1);
    assert(FakePreferences.strings.count("pass") == 1);
    FakePreferences.putBytesLimit = 112;
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Power cut SSID");
    assert(FakePreferences.strings.empty());
  }

  resetFakeRuntime();
  {
    seedLegacy("Cleanup SSID", "cleanup-pass");
    FakePreferences.removeResult = false;
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(FakePreferences.bytes.count("cred_blob") == 1);
    assert(FakePreferences.strings.size() == 2);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Cleanup SSID");
    assert(FakePreferences.strings.empty());
  }

  resetFakeRuntime();
  {
    seedLegacy("Old SSID", "old-password");
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    const std::vector<uint8_t> oldRecord = FakePreferences.bytes["cred_blob"];
    FakePreferences.putBytesLimit = 17;
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("New SSID"), String("new-password")));
    assert(std::string(portal.savedSSID().c_str()) == "Old SSID");
    assert(FakePreferences.bytes["cred_blob"] == oldRecord);
    assert(FakePreferences.bytes["cred_backup"].size() == 17);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Old SSID");
    assert(FakePreferences.bytes.count("cred_backup") == 0);
  }

  // Power loss after a verified backup but during the primary update restores
  // the complete previous credentials, never a partial new record.
  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Stable SSID"), String("stable-password")));
    FakePreferences.putBytesLimits = {112, 23};
    assert(!ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Interrupted SSID"), String("interrupted-password")));
    assert(FakePreferences.bytes["cred_blob"].size() == 23);
    assert(FakePreferences.bytes["cred_backup"].size() == 112);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "Stable SSID");
    assert(FakePreferences.bytes["cred_blob"].size() == 112);
    assert(FakePreferences.bytes.count("cred_backup") == 0);
  }

  // If reset occurs after the new primary is verified but before backup
  // cleanup, the valid primary is authoritative.
  resetFakeRuntime();
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("Old primary"), String("old-primary-password")));
    FakePreferences.removeResult = false;
    assert(ESP32WiFiPortalTestAccess::saveCredentials(
        portal, String("New primary"), String("new-primary-password")));
    assert(FakePreferences.bytes.count("cred_blob") == 1);
    assert(FakePreferences.bytes.count("cred_backup") == 1);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.hasSavedCredentials());
    assert(std::string(portal.savedSSID().c_str()) == "New primary");
    assert(FakePreferences.bytes.count("cred_backup") == 0);
  }

  // Power loss after the primary is deleted but before the backup is deleted
  // leaves the erase marker authoritative. Reboot must finish deletion instead
  // of restoring the surviving backup.
  resetFakeRuntime();
  {
    FakePreferences.bytes["cred_blob"] = valid;
    FakePreferences.bytes["cred_backup"] =
        makeRecord("Backup SSID", "backup-password");
    seedLegacy("Legacy SSID", "legacy-password");
    FakePreferences.removeResults = {true, false, true, true};
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.eraseCredentials(false));
    assert(FakePreferences.bytes.count("cred_blob") == 0);
    assert(FakePreferences.bytes.count("cred_backup") == 1);
    assert(FakePreferences.bytes.count("cred_erased") == 1);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::NotFound);
    assert(FakePreferences.bytes.empty());
    assert(FakePreferences.strings.empty());
  }

  // Even a short marker produced by an interrupted marker write is treated
  // fail-closed. Existing records are not resurrected after reset.
  resetFakeRuntime();
  {
    FakePreferences.bytes["cred_blob"] = valid;
    FakePreferences.putBytesLimit = 2;
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.eraseCredentials(false));
    assert(FakePreferences.bytes.count("cred_blob") == 1);
    assert(FakePreferences.bytes["cred_erased"].size() == 2);
  }
  resetFakeRuntime(true);
  {
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::NotFound);
    assert(FakePreferences.bytes.empty());
  }

  resetFakeRuntime();
  {
    FakePreferences.bytes["cred_blob"] = valid;
    seedLegacy("Legacy", "legacy-password");
    ESP32WiFiPortal portal;
    portal.setLogging(false);
    assert(portal.eraseCredentials(false));
    assert(FakePreferences.bytes.empty());
    assert(FakePreferences.strings.empty());
    assert(!portal.hasSavedCredentials());
    assert(ESP32WiFiPortalTestAccess::credentialStatus(portal) ==
           CredentialStatus::NotFound);
  }

  std::cout << "Credential CRC, blob, migration and power-loss tests passed\n";
  return 0;
}
