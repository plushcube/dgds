find_package(Threads REQUIRED)

dgds_add_test_area(stubs blob_store_test.cpp device_key_test.cpp
                          identity_registry_test.cpp key_store_test.cpp
                          metadata_registry_test.cpp receipt_store_test.cpp
                          store_separation_test.cpp)

target_link_libraries(dgds-test-stubs PRIVATE dgds::stub::blob-store
                                              dgds::stub::device-key
                                              dgds::stub::identity-registry
                                              dgds::stub::key-store
                                              dgds::stub::metadata-registry
                                              dgds::stub::receipt-store
                                              Threads::Threads)
