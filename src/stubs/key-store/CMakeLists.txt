add_library(dgds-stub-key-store STATIC "src/file_key_store.cpp")
add_library(dgds::stub::key-store ALIAS dgds-stub-key-store)

target_include_directories(
  dgds-stub-key-store
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-key-store
                      PUBLIC dgds::core
                      PRIVATE dgds::stub::support dgds::warnings)
