dgds_add_test_area(server api_surface_test.cpp
                          live_server_test.cpp
                          http_binding_test.cpp
                          attribution_service_test.cpp
                          catalog_service_test.cpp
                          delivery_service_test.cpp
                          publication_service_test.cpp
                          purchase_service_test.cpp
                          user_service_test.cpp)

target_link_libraries(dgds-test-server PRIVATE dgds::server nlohmann_json::nlohmann_json
                                               dgds::stub::blob-store
                                               dgds::stub::identity-registry
                                               dgds::stub::key-store
                                               dgds::stub::metadata-registry)

add_dependencies(dgds-test-server ${TASK_CLI})
target_compile_definitions(dgds-test-server PRIVATE DGDS_SERVER_PATH="$<TARGET_FILE:${TASK_CLI}>")
