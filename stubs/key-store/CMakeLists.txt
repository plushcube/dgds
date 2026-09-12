add_library(dgds-stub-key-store INTERFACE)
add_library(dgds::stub::key-store ALIAS dgds-stub-key-store)

target_link_libraries(dgds-stub-key-store INTERFACE dgds::core)
