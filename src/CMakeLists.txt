add_executable(${TASK_CLI} "main.cpp")

target_include_directories(${TASK_CLI} PRIVATE "${PROJECT_BINARY_DIR}/include")
target_link_libraries(${TASK_CLI} PRIVATE dgds::server dgds::warnings)
