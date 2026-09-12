add_executable(dgds-example "src/main.cpp")

target_include_directories(dgds-example PRIVATE "${PROJECT_BINARY_DIR}/include")
target_link_libraries(dgds-example PRIVATE dgds::client dgds::warnings)
