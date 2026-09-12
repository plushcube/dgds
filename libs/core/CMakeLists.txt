add_library(dgds-core INTERFACE)
add_library(dgds::core ALIAS dgds-core)

target_include_directories(
  dgds-core INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-core INTERFACE OpenSSL::Crypto)
