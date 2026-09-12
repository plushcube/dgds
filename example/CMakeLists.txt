add_executable(dgds-example "src/main.cpp")

target_link_libraries(dgds-example PRIVATE dgds::client dgds::warnings)
