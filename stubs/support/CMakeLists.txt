add_library(dgds-stub-support STATIC "src/file_storage.cpp")
add_library(dgds::stub::support ALIAS dgds-stub-support)

target_include_directories(
  dgds-stub-support
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-support
                      PUBLIC dgds::core
                      PRIVATE dgds::warnings)
