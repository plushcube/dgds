set(client_tests api_contract_test.cpp client_test.cpp example_run_test.cpp
                 pinned_connection_test.cpp)

if(APPLE)
  list(APPEND client_tests keychain_device_key_test.cpp keychain_receipt_store_test.cpp)
endif()

dgds_add_test_area(client ${client_tests})

# dgds::client приходит транзитивно через заглушку транспорта, которая наследует его порт
target_link_libraries(dgds-test-client
                      PRIVATE dgds::stub::direct-transport dgds::stub::blob-store dgds::stub::device-key
                              dgds::stub::identity-registry dgds::stub::key-store dgds::stub::metadata-registry
                              dgds::stub::receipt-store)

target_include_directories(dgds-test-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)

add_dependencies(dgds-test-client dgds-example)
target_compile_definitions(dgds-test-client PRIVATE DGDS_EXAMPLE_PATH="$<TARGET_FILE:dgds-example>")
