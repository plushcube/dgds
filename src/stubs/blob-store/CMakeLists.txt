add_library(dgds-stub-blob-store STATIC "src/file_blob_store.cpp")
add_library(dgds::stub::blob-store ALIAS dgds-stub-blob-store)

target_include_directories(
  dgds-stub-blob-store
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-blob-store
                      PUBLIC dgds::core
                      PRIVATE dgds::stub::support dgds::warnings)
