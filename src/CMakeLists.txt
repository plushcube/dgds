set(DGDS_BACKEND
    "stubs"
    CACHE STRING "Набор реализаций серверных портов: stubs | services"
)
set_property(CACHE DGDS_BACKEND PROPERTY STRINGS stubs services)

add_executable(${SERVER_APP} "server.cpp" "composition/configuration.cpp" "composition/tls.cpp")
add_executable(${CLIENT_APP} "client.cpp")

target_include_directories(${SERVER_APP} PRIVATE "${PROJECT_BINARY_DIR}/include")
target_include_directories(${CLIENT_APP} PRIVATE "${PROJECT_BINARY_DIR}/include")

if(DGDS_BACKEND STREQUAL "stubs")
  add_subdirectory(stubs)

  target_sources(${SERVER_APP} PRIVATE "composition/stubbed_ports.cpp")
  target_link_libraries(
    ${SERVER_APP} PRIVATE dgds::server dgds::stub::blob-store dgds::stub::identity-registry
                          dgds::stub::key-store dgds::stub::metadata-registry
  )
  target_compile_definitions(${SERVER_APP} PRIVATE DGDS_BACKEND_NAME="stubs")

  # dgds::client приходит транзитивно через заглушки устройства
  target_link_libraries(${CLIENT_APP} PRIVATE dgds::stub::device-key dgds::stub::receipt-store)

elseif(DGDS_BACKEND STREQUAL "services")
  message(FATAL_ERROR "Набор «services» не реализован")
else()
  message(
    FATAL_ERROR "Неизвестный DGDS_BACKEND: ${DGDS_BACKEND}. Допустимые значения: stubs, services."
  )
endif()

target_link_libraries(${SERVER_APP} PRIVATE dgds::warnings)
target_link_libraries(${CLIENT_APP} PRIVATE dgds::warnings)
