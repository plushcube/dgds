add_library(dgds-client STATIC "src/client.cpp"
                             "src/http/http_transport.cpp"
                             "src/http/pinned_client.cpp"
                             "src/http/server_trust.cpp")
add_library(dgds::client ALIAS dgds-client)

target_include_directories(
  dgds-client PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-client
                      PUBLIC dgds::core httplib::httplib
                      PRIVATE dgds::warnings nlohmann_json::nlohmann_json)

if(APPLE)
  add_library(dgds-client-macos STATIC "src/platform/macos/keychain_device_key.cpp"
                                       "src/platform/macos/keychain_receipt_store.cpp")
  add_library(dgds::client::macos ALIAS dgds-client-macos)

  target_include_directories(
    dgds-client-macos
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries(dgds-client-macos
                        PUBLIC dgds::core "-framework CoreFoundation" "-framework Security"
                        PRIVATE dgds::warnings)

  target_link_libraries(dgds-client PUBLIC dgds-client-macos)
endif()

dgds_apply_version(dgds-client client 0.1.0)

if(APPLE)
  dgds_apply_version(dgds-client-macos client-macos 0.1.0)
endif()
