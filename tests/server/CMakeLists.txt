dgds_add_test_area(server publication_service_test.cpp user_service_test.cpp)

target_link_libraries(dgds-test-server PRIVATE dgds::server dgds::stub::blob-store
                                               dgds::stub::identity-registry
                                               dgds::stub::key-store
                                               dgds::stub::metadata-registry)
