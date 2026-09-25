add_executable(dgds-example "src/main.cpp")

target_include_directories(dgds-example PRIVATE "${PROJECT_BINARY_DIR}/include")
# dgds::client приходит транзитивно через заглушки устройства
target_link_libraries(dgds-example
                      PRIVATE dgds::stub::device-key dgds::stub::receipt-store dgds::warnings)
