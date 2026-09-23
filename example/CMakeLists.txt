add_executable(dgds-example "src/main.cpp")

target_include_directories(dgds-example PRIVATE "${PROJECT_BINARY_DIR}/include")
target_link_libraries(dgds-example
                      PRIVATE dgds::stub::direct-transport dgds::stub::blob-store
                              dgds::stub::device-key dgds::stub::identity-registry
                              dgds::stub::key-store dgds::stub::metadata-registry
                              dgds::stub::receipt-store dgds::warnings)
