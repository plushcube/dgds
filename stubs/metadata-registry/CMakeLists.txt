add_library(dgds-stub-metadata-registry INTERFACE)
add_library(dgds::stub::metadata-registry ALIAS dgds-stub-metadata-registry)

target_link_libraries(dgds-stub-metadata-registry INTERFACE dgds::core)
