add_library(dgds-stub-identity-registry STATIC "src/file_identity_registry.cpp")
add_library(dgds::stub::identity-registry ALIAS dgds-stub-identity-registry)

target_include_directories(
  dgds-stub-identity-registry
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-identity-registry
                      PUBLIC dgds::core
                      PRIVATE dgds::warnings)
