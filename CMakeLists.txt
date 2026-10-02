cmake_minimum_required(VERSION 3.28)

set(PATCH_VERSION
    "1"
    CACHE STRING "Номер сборки в версии продукта"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("DGDS" VERSION 0.0.${PATCH_VERSION})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

set(CLIENT_APP "dgds-client")
set(SERVER_APP "dgds-server")

option(WITH_TESTS "Whether to build tests" ON)

set(DGDS_SANITIZER
    "none"
    CACHE STRING "Санитайзеры сборки: none, address, undefined, thread или их список через запятую"
)
set_property(
  CACHE DGDS_SANITIZER PROPERTY STRINGS none address undefined thread "address,undefined"
)

if(WITH_TESTS)
  enable_testing()
endif()

include(cmake/dependencies.cmake)
include(cmake/version.cmake)
include(cmake/tests.cmake)

include(GNUInstallDirs)

dgds_generate_product_version("${PROJECT_VERSION}")

add_library(dgds-warnings INTERFACE)
add_library(dgds::warnings ALIAS dgds-warnings)

# Санитайзеры вынесены в отдельный интерфейс и подключены к dgds::warnings, поэтому флаги попадают и
# в библиотеки, и в приложения, и в тесты без правки их целей.
add_library(dgds-sanitizers INTERFACE)
add_library(dgds::sanitizers ALIAS dgds-sanitizers)

if(NOT DGDS_SANITIZER STREQUAL "none")
  if(MSVC)
    message(FATAL_ERROR "Санитайзеры DGDS_SANITIZER поддерживаются только GCC и Clang")
  endif()

  target_compile_options(
    dgds-sanitizers INTERFACE -fsanitize=${DGDS_SANITIZER} -fno-omit-frame-pointer -g
  )
  target_link_options(dgds-sanitizers INTERFACE -fsanitize=${DGDS_SANITIZER})
endif()

if(MSVC)
  target_compile_options(dgds-warnings INTERFACE /W4)
else()
  target_compile_options(dgds-warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

target_link_libraries(dgds-warnings INTERFACE dgds-sanitizers)

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(tools)

install(TARGETS ${CLIENT_APP} RUNTIME DESTINATION bin)
install(TARGETS ${SERVER_APP} RUNTIME DESTINATION bin)

install(FILES README.md LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES packaging/QUICKSTART.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(
  DIRECTORY docs/
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs"
  FILES_MATCHING
  PATTERN "*.md"
  PATTERN "*.svg"
)

if(WITH_TESTS)
  add_subdirectory(tests)
endif()

include(cmake/packaging.cmake)
include(CPack)
