add_library(dgds-stub-identity-registry INTERFACE)
add_library(dgds::stub::identity-registry ALIAS dgds-stub-identity-registry)

target_link_libraries(dgds-stub-identity-registry INTERFACE dgds::core)
