set(DGDS_BACKEND
    "stubs"
    CACHE STRING "Набор реализаций серверных портов: stubs | services")
set_property(CACHE DGDS_BACKEND PROPERTY STRINGS stubs services)

add_executable(${TASK_CLI} "main.cpp" "composition/configuration.cpp" "composition/tls.cpp")

target_include_directories(${TASK_CLI} PRIVATE "${PROJECT_BINARY_DIR}/include")

if(DGDS_BACKEND STREQUAL "stubs")
  target_sources(${TASK_CLI} PRIVATE "composition/server_ports.cpp")
  target_link_libraries(${TASK_CLI} PRIVATE dgds::server
                                              dgds::stub::blob-store
                                              dgds::stub::identity-registry
                                              dgds::stub::key-store
                                              dgds::stub::metadata-registry)
  target_compile_definitions(${TASK_CLI} PRIVATE DGDS_BACKEND_NAME="stubs")
elseif(DGDS_BACKEND STREQUAL "services")
  message(
    FATAL_ERROR
      "Набор «services» не реализован: серверные сервисы — работа целевого продукта, "
      "см. раздел о переходе в design.md. Собирайте с -DDGDS_BACKEND=stubs.")
else()
  message(FATAL_ERROR "Неизвестный DGDS_BACKEND: ${DGDS_BACKEND}. Допустимые значения: stubs, services.")
endif()

target_link_libraries(${TASK_CLI} PRIVATE dgds::warnings)
