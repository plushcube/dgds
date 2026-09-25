add_library(dgds-test-support STATIC "server_process.cpp")
add_library(dgds::test-support ALIAS dgds-test-support)

target_include_directories(
  dgds-test-support PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(dgds-test-support PUBLIC GTest::gtest)

# Живые тесты поднимают настоящий сервер: путь к нему подставляется сборкой
add_dependencies(dgds-test-support ${TASK_CLI})
target_compile_definitions(dgds-test-support PUBLIC DGDS_SERVER_PATH="$<TARGET_FILE:${TASK_CLI}>")
