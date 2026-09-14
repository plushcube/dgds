cmake_minimum_required(VERSION 3.12)
set(CMAKE_POLICY_VERSION_MINIMUM 3.12)

set(PATCH_VERSION
    "1"
    CACHE INTERNAL "Patch version")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("DGDS" VERSION 0.0.${PATCH_VERSION})

set(TASK_CLI "dgds")

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
add_subdirectory(stubs)
add_subdirectory(src)
add_subdirectory(example)

install(TARGETS ${TASK_CLI} RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME ${TASK_CLI})
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "https://t.me/plushcube")
include(CPack)

if(WITH_TESTS)
  add_subdirectory(tests)
endif()
