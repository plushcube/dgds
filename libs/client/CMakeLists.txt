add_library(dgds-client INTERFACE)
add_library(dgds::client ALIAS dgds-client)

target_include_directories(
  dgds-client INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-client INTERFACE dgds::core httplib::httplib
                                             nlohmann_json::nlohmann_json)

if(APPLE)
  add_library(dgds-client-macos STATIC "src/platform/macos/keychain_device_key.cpp")
  add_library(dgds::client::macos ALIAS dgds-client-macos)

  target_include_directories(
    dgds-client-macos
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries(dgds-client-macos
                        PUBLIC dgds::core
                        PRIVATE dgds::warnings)
  target_link_libraries(dgds-client-macos PRIVATE "-framework Security")

  target_link_libraries(dgds-client INTERFACE dgds-client-macos)
endif()

dgds_apply_version(dgds-client client 0.1.0)

if(APPLE)
  dgds_apply_version(dgds-client-macos client-macos 0.1.0)
endif()
