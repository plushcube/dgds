dgds_add_test_area(client keychain_device_key_test.cpp)

target_link_libraries(dgds-test-client PRIVATE dgds::client)
