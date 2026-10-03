set(CLIENT_TEST_APP "dgds-test-client")

set(CLIENT_TEST_SOURCES
    "api_contract_test.cpp" "client_test.cpp" "example_run_test.cpp" "http_transport_test.cpp"
    "key_separation_test.cpp" "pinned_connection_test.cpp"
)

if(APPLE)
  list(APPEND CLIENT_TEST_SOURCES "keychain_device_key_test.cpp" "keychain_receipt_store_test.cpp")
endif()

dgds_add_test_area(client ${CLIENT_TEST_SOURCES})

target_link_libraries(
  ${CLIENT_TEST_APP}
  PRIVATE dgds::stub::direct-transport
          dgds::stub::blob-store
          dgds::stub::device-key
          dgds::stub::identity-registry
          dgds::stub::key-store
          dgds::stub::metadata-registry
          dgds::stub::receipt-store
)

target_include_directories(${CLIENT_TEST_APP} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)

add_dependencies(${CLIENT_TEST_APP} ${CLIENT_APP})
target_compile_definitions(
  ${CLIENT_TEST_APP} PRIVATE DGDS_CLIENT_PATH="$<TARGET_FILE:${CLIENT_APP}>"
)
