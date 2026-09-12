add_library(dgds-client INTERFACE)
add_library(dgds::client ALIAS dgds-client)

target_include_directories(
  dgds-client INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-client INTERFACE dgds::core)
