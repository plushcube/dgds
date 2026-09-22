add_library(dgds-stub-device-key STATIC "src/file_device_key.cpp")
add_library(dgds::stub::device-key ALIAS dgds-stub-device-key)

target_include_directories(
  dgds-stub-device-key
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-device-key
                      PUBLIC dgds::client
                      PRIVATE dgds::stub::support dgds::warnings)
