add_library(dgds-stub-device-key INTERFACE)
add_library(dgds::stub::device-key ALIAS dgds-stub-device-key)

target_link_libraries(dgds-stub-device-key INTERFACE dgds::client)
