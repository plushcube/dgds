dgds_add_test_area(server user_service_test.cpp)

target_link_libraries(dgds-test-server PRIVATE dgds::server dgds::stub::metadata-registry)
