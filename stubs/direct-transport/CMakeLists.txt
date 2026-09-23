add_library(dgds-stub-direct-transport STATIC "src/direct_transport.cpp")
add_library(dgds::stub::direct-transport ALIAS dgds-stub-direct-transport)

target_include_directories(
  dgds-stub-direct-transport
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-direct-transport
                      PUBLIC dgds::client dgds::server
                      PRIVATE dgds::warnings)
