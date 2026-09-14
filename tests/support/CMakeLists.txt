add_library(dgds-test-support INTERFACE)
add_library(dgds::test-support ALIAS dgds-test-support)

target_include_directories(
  dgds-test-support INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(dgds-test-support INTERFACE GTest::gtest)
