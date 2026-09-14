add_library(dgds-core STATIC "src/identity/canonical_form.cpp"
                             "src/identity/content_identity.cpp"
                             "src/watermark/mark_codec.cpp")
add_library(dgds::core ALIAS dgds-core)

target_include_directories(
  dgds-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-core PUBLIC OpenSSL::Crypto)
target_link_libraries(dgds-core PRIVATE dgds::warnings)

dgds_apply_version(dgds-core core 0.1.0)
