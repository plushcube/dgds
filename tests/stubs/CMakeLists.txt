find_package(Threads REQUIRED)

dgds_add_test_area(stubs device_key_test.cpp identity_registry_test.cpp
                          receipt_store_test.cpp)

target_link_libraries(dgds-test-stubs PRIVATE dgds::stub::device-key
                                              dgds::stub::identity-registry
                                              dgds::stub::receipt-store
                                              Threads::Threads)
