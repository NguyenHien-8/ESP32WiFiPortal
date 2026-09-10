#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include <PortalPage.h>

#include <cassert>
#include <iostream>
#include <string>

namespace {

void expectSaveValidation(const std::string& ssid,
                          const std::string& password,
                          int expectedStatus,
                          const std::string& expectedSSID = std::string()) {
  resetFakeRuntime();
  ESP32WiFiPortal portal;
  portal.setLogging(false);
  assert(portal.startConfigPortalAsync("Portal-Test", "12345678"));
  FakeWebServer.args["ssid"] = ssid;
  FakeWebServer.args["password"] = password;
  FakeWebServer.handlers.at("/save")();
  assert(FakeWebServer.lastStatus == expectedStatus);
  if (expectedStatus == 200) {
    assert(std::string(
               ESP32WiFiPortalTestAccess::pendingSSID(portal).c_str()) ==
           expectedSSID);
    FakeWebServer.handlers.at("/save")();
    assert(FakeWebServer.lastStatus == 409);
  }
  portal.stopConfigPortal();
}

}  // namespace

int main() {
  const std::string page(EWP_PORTAL_HTML);
  const std::string brandAndLink =
      "<p class=\"brand\">ESP32 WiFi Portal</p><a class=\"advanced-link\" "
      "id=\"advancedLink\" href=\"#advanced\">Advanced Wi-Fi Setting</a>";
  assert(page.find(brandAndLink) != std::string::npos);
  assert(page.find("id=\"advancedView\"") != std::string::npos);
  assert(page.find("id=\"advancedBack\" href=\"#scan\">Back</a>") !=
         std::string::npos);
  assert(page.find("Connect and save") != std::string::npos);
  assert(page.find("method=\"post\" action=\"/save\"") !=
         std::string::npos);
  assert(page.find("manualReveal.onclick") != std::string::npos);
  assert(page.find("if(submitting)return") != std::string::npos);
  assert(page.find("aria-pressed") != std::string::npos);
  assert(page.find("text-decoration:underline") != std::string::npos);
  assert(page.find("width:36%") != std::string::npos);
  assert(page.find("1.15s linear infinite") != std::string::npos);
  assert(page.find("prefers-reduced-motion:reduce") != std::string::npos);
  assert(page.find("scaleX(") == std::string::npos);
  assert(page.find("sessionStorage.setItem('ewpPassword") ==
         std::string::npos);
  assert(page.find("querySelectorAll('button,input')") ==
         std::string::npos);

  expectSaveValidation(" Hidden SSID ", "", 200, " Hidden SSID ");
  expectSaveValidation(std::string(32, 'S'), std::string(63, 'P'), 200,
                       std::string(32, 'S'));
  expectSaveValidation("Secured", "12345678", 200, "Secured");
  expectSaveValidation("", "", 400);
  expectSaveValidation(std::string(33, 'S'), "12345678", 400);
  expectSaveValidation("Secured", "1234567", 400);
  expectSaveValidation("Secured", std::string(64, 'P'), 400);

  std::cout << "Advanced portal UI and exact credential validation tests passed\n";
  return 0;
}
