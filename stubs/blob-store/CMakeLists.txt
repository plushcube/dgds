add_library(dgds-stub-blob-store INTERFACE)
add_library(dgds::stub::blob-store ALIAS dgds-stub-blob-store)

target_link_libraries(dgds-stub-blob-store INTERFACE dgds::core)
