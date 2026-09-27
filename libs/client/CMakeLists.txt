set(CLIENT_LIB "dgdsclnt")

add_library(${CLIENT_LIB} STATIC
  "src/client.cpp"
  "src/http/http_transport.cpp"
  "src/http/pinned_client.cpp"
  "src/http/server_trust.cpp"
)
add_library(dgds::client ALIAS ${CLIENT_LIB})

target_include_directories(
  ${CLIENT_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(${CLIENT_LIB}
  PUBLIC dgds::core httplib::httplib
  PRIVATE dgds::warnings nlohmann_json::nlohmann_json
)

if(APPLE)
  add_library(${CLIENT_LIB}-macos STATIC
    "src/platform/macos/keychain_device_key.cpp"
    "src/platform/macos/keychain_receipt_store.cpp"
  )
  add_library(dgds::client::macos ALIAS ${CLIENT_LIB}-macos)

  target_include_directories(${CLIENT_LIB}-macos
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )

  target_link_libraries(${CLIENT_LIB}-macos
    PUBLIC dgds::core "-framework CoreFoundation" "-framework Security"
    PRIVATE dgds::warnings
  )

  target_link_libraries(${CLIENT_LIB} PUBLIC ${CLIENT_LIB}-macos)
endif()

dgds_apply_version(${CLIENT_LIB} client 0.1.0)

if(APPLE)
  dgds_apply_version(${CLIENT_LIB}-macos client-macos 0.1.0)
endif()
