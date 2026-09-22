add_library(dgds-server STATIC "src/services/catalog_service.cpp"
                               "src/services/delivery_service.cpp"
                               "src/services/publication_service.cpp"
                               "src/services/purchase_service.cpp"
                               "src/services/session_store.cpp"
                               "src/services/user_service.cpp")
add_library(dgds::server ALIAS dgds-server)

target_include_directories(
  dgds-server PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-server
                      PUBLIC dgds::core
                      PRIVATE dgds::warnings)

dgds_apply_version(dgds-server server 0.1.0)
