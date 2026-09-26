add_executable(dgds-loadgen "loadgen/main.cpp")

target_link_libraries(dgds-loadgen PRIVATE httplib::httplib nlohmann_json::nlohmann_json dgds::warnings)

if(APPLE)
  target_link_libraries(dgds-loadgen PRIVATE "-framework CoreFoundation" "-framework Security")
endif()
