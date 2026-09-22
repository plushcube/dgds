add_library(dgds-core STATIC "src/codec/binary.cpp"
                             "src/crypto/aead.cpp"
                             "src/crypto/sealed_content_codec.cpp"
                             "src/crypto/secure_buffer.cpp"
                             "src/envelope/device_wrap.cpp"
                             "src/envelope/keys.cpp"
                             "src/envelope/package.cpp"
                             "src/envelope/receipt.cpp"
                             "src/identity/canonical_form.cpp"
                             "src/identity/content_identity.cpp"
                             "src/identity/user_id.cpp"
                             "src/signature/author_signature.cpp"
                             "src/watermark/mark_codec.cpp")
add_library(dgds::core ALIAS dgds-core)

target_include_directories(
  dgds-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-core PUBLIC OpenSSL::Crypto)
target_link_libraries(dgds-core PRIVATE dgds::warnings)

dgds_apply_version(dgds-core core 0.1.0)
