cmake_minimum_required(VERSION 3.12)
set(CMAKE_POLICY_VERSION_MINIMUM 3.12)

set(PATCH_VERSION
    "1"
    CACHE INTERNAL "Patch version")
set(PROJECT_VERSION "0.0.${PATCH_VERSION}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("DGDS" VERSION ${PROJECT_VERSION})

set(TASK_CLI "dgds")
set(TASK_TEST "test_dgds")

option(WITH_TESTS "Whether to build tests" ON)

if(WITH_TESTS)
  enable_testing()
endif()

include(cmake/dependencies.cmake)

add_library(dgds-warnings INTERFACE)
add_library(dgds::warnings ALIAS dgds-warnings)

if(MSVC)
  target_compile_options(dgds-warnings INTERFACE /W4)
else()
  target_compile_options(dgds-warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(libs)
add_subdirectory(stubs)
add_subdirectory(src)
add_subdirectory(example)

configure_file("${PROJECT_SOURCE_DIR}/version.h.in"
               "${PROJECT_BINARY_DIR}/version.h")

install(TARGETS ${TASK_CLI} RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME ${TASK_CLI})
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "https://t.me/plushcube")
include(CPack)

if(WITH_TESTS)
  add_executable(${TASK_TEST} "tests/test_dgds.cpp")
  target_link_libraries(${TASK_TEST} PRIVATE GTest::gtest_main dgds::warnings)

  include(GoogleTest)
  gtest_discover_tests(${TASK_TEST})
endif()
