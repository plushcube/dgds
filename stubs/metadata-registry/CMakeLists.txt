add_library(dgds-stub-metadata-registry STATIC "src/file_metadata_registry.cpp"
                                               "src/records.cpp")
add_library(dgds::stub::metadata-registry ALIAS dgds-stub-metadata-registry)

target_include_directories(
  dgds-stub-metadata-registry
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(dgds-stub-metadata-registry
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dgds-stub-metadata-registry
                      PUBLIC dgds::core
                      PRIVATE dgds::stub::support dgds::warnings)
