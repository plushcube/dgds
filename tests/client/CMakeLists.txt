set(client_tests client_test.cpp)

if(APPLE)
  list(APPEND client_tests keychain_device_key_test.cpp)
endif()

dgds_add_test_area(client ${client_tests})

# dgds::client приходит транзитивно через заглушку транспорта, которая наследует его порт
target_link_libraries(dgds-test-client
                      PRIVATE dgds::stub::direct-transport dgds::stub::blob-store dgds::stub::device-key
                              dgds::stub::identity-registry dgds::stub::key-store dgds::stub::metadata-registry
                              dgds::stub::receipt-store)
