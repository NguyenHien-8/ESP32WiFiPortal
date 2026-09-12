#include "host/FakeRuntime.h"
#include "host/TestAccess.h"

#include <PortalPage.h>

#include "host/TestAssert.h"
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

size_t countOccurrences(const std::string& value,
                        const std::string& expected) {
  size_t count = 0;
  size_t position = 0;
  while ((position = value.find(expected, position)) != std::string::npos) {
    ++count;
    position += expected.length();
  }
  return count;
}

}  // namespace

int main() {
  const std::string page(EWP_PORTAL_HTML);
  assert(page.find("id=\"advancedLink\" href=\"#advanced\">More Wi-Fi settings</a>") !=
         std::string::npos);
  assert(page.find("id=\"advancedView\"") != std::string::npos);
  assert(page.find("id=\"manualView\"") != std::string::npos);
  assert(page.find("id=\"propertiesView\"") != std::string::npos);
  assert(page.find("id=\"advancedBack\" href=\"#scan\">Back Home Screen</a>") !=
         std::string::npos);
  assert(page.find("id=\"manualButton\" type=\"button\">Manual Configure WiFi</button>") !=
         std::string::npos);
  assert(page.find("id=\"propertiesButton\" type=\"button\">Properties</button>") !=
         std::string::npos);
  assert(page.find(">Reset</button>") != std::string::npos);
  assert(page.find("id=\"manualBack\" href=\"#advanced\">Back</a>") !=
         std::string::npos);
  assert(page.find("Manual Configure Wi-Fi") != std::string::npos);
  assert(page.find("Network name") != std::string::npos);
  assert(page.find("Password") != std::string::npos);
  assert(page.find(">Connect</button>") != std::string::npos);
  assert(page.find("id=\"propertiesBack\" href=\"#advanced\">Back</a>") !=
         std::string::npos);
  assert(page.find("Properties Device") != std::string::npos);

  const size_t advancedStart = page.find("id=\"advancedView\"");
  const size_t manualStart = page.find("id=\"manualView\"");
  assert(advancedStart < manualStart);
  const std::string advancedView =
      page.substr(advancedStart, manualStart - advancedStart);
  assert(advancedView.find("Network name") == std::string::npos);
  assert(countOccurrences(advancedView, "<button") == 3);
  assert(page.find("manualButton.onclick=()=>location.hash='#manual'") !=
         std::string::npos);
  assert(page.find("propertiesButton.onclick=()=>location.hash='#properties'") !=
         std::string::npos);

  assert(page.find("method=\"post\" action=\"/save\"") !=
         std::string::npos);
  assert(page.find("id=\"manualPassword\" name=\"manual-password\" type=\"password\"") !=
         std::string::npos);
  assert(page.find("id=\"manualPassword\" name=\"manual-password\" type=\"password\" maxlength=\"64\"") !=
         std::string::npos);
  assert(page.find("input.maxLength=64") != std::string::npos);
  assert(page.find("^[0-9a-f]{64}$") != std::string::npos);
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
  assert(page.find("localStorage") == std::string::npos);
  assert(page.find("URLSearchParams") == std::string::npos);
  assert(page.find("innerHTML") == std::string::npos);
  assert(page.find("fetch('/properties',{cache:'no-store'})") !=
         std::string::npos);
  assert(page.find("document.getElementById(id).textContent=value") !=
         std::string::npos);
  assert(page.find("fetch('/reset',{method:'POST',cache:'no-store'})") !=
         std::string::npos);
  assert(page.find("showView();\nscan();") == std::string::npos);
  assert(page.find("if(view==='scan'&&!scanInitialized)scan()") !=
         std::string::npos);
  assert(page.find("history.replaceState(null,'','#scan')") !=
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
  expectSaveValidation("Raw PSK", std::string(64, 'A'), 200, "Raw PSK");
  expectSaveValidation("Secured", std::string(65, 'A'), 400);

  std::cout << "Advanced portal UI and exact credential validation tests passed\n";
  return 0;
}
