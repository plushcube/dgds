cmake_minimum_required(VERSION 3.28)

set(PATCH_VERSION
    "1"
    CACHE INTERNAL "Patch version")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("DGDS" VERSION 0.0.${PATCH_VERSION})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

set(CLIENT_APP "dgds-client")
set(SERVER_APP "dgds-server")

option(WITH_TESTS "Whether to build tests" ON)

if(WITH_TESTS)
  enable_testing()
endif()

include(cmake/dependencies.cmake)
include(cmake/version.cmake)
include(cmake/tests.cmake)

dgds_generate_product_version("${PROJECT_VERSION}")

add_library(dgds-warnings INTERFACE)
add_library(dgds::warnings ALIAS dgds-warnings)

if(MSVC)
  target_compile_options(dgds-warnings INTERFACE /W4)
else()
  target_compile_options(dgds-warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(tools)

install(TARGETS ${CLIENT_APP} RUNTIME DESTINATION bin)
install(TARGETS ${SERVER_APP} RUNTIME DESTINATION bin)

if(WITH_TESTS)
  add_subdirectory(tests)
endif()
