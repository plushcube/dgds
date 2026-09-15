find_package(Threads REQUIRED)

dgds_add_test_area(stubs identity_registry_test.cpp)

target_link_libraries(dgds-test-stubs PRIVATE dgds::stub::identity-registry
                                              Threads::Threads)
