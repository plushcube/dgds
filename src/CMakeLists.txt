add_executable(${TASK_CLI} "main.cpp")

target_link_libraries(${TASK_CLI} PRIVATE dgds::server dgds::warnings)
