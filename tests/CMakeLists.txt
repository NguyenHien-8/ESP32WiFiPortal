cmake_minimum_required(VERSION 3.16)
project(ESP32WiFiPortalHostTests LANGUAGES CXX)

enable_testing()

add_library(fake_esp32_portal STATIC
  host/FakeRuntime.cpp
  ../src/ESP32WiFiPortal.cpp
)
target_compile_features(fake_esp32_portal PUBLIC cxx_std_11)
target_compile_definitions(fake_esp32_portal PUBLIC
  ESP32=1
  ESP32WIFIPORTAL_ENABLE_TEST_ACCESS=1
)
target_include_directories(fake_esp32_portal PUBLIC
  host/include
  ../src
)

if(MSVC)
  target_compile_options(fake_esp32_portal PRIVATE /W4 /permissive-)
else()
  target_compile_options(fake_esp32_portal PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(portal_network_validation PortalNetworkValidationTest.cpp)
target_link_libraries(portal_network_validation PRIVATE fake_esp32_portal)
add_test(NAME portal_network_validation COMMAND portal_network_validation)

add_executable(portal_lifecycle PortalLifecycleTest.cpp)
target_link_libraries(portal_lifecycle PRIVATE fake_esp32_portal)
add_test(NAME portal_lifecycle COMMAND portal_lifecycle)

add_executable(credential_storage CredentialStorageTest.cpp)
target_link_libraries(credential_storage PRIVATE fake_esp32_portal)
add_test(NAME credential_storage COMMAND credential_storage)

add_executable(auto_reconnect AutoReconnectTest.cpp)
target_link_libraries(auto_reconnect PRIVATE fake_esp32_portal)
add_test(NAME auto_reconnect COMMAND auto_reconnect)

add_executable(portal_page PortalPageTest.cpp)
target_link_libraries(portal_page PRIVATE fake_esp32_portal)
add_test(NAME portal_page COMMAND portal_page)

add_executable(header_odr
  HeaderODRTest.cpp
  HeaderODRPartA.cpp
  HeaderODRPartB.cpp
)
target_link_libraries(header_odr PRIVATE fake_esp32_portal)
add_test(NAME header_odr COMMAND header_odr)
