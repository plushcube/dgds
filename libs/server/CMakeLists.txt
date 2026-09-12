add_library(dgds-server INTERFACE)
add_library(dgds::server ALIAS dgds-server)

target_include_directories(
  dgds-server INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-server INTERFACE dgds::core)
