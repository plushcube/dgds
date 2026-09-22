dgds_add_test_area(core aead_test.cpp author_signature_test.cpp
                        canonical_form_test.cpp content_identity_test.cpp
                        device_wrap_test.cpp keys_test.cpp
                        mark_codec_test.cpp package_test.cpp
                        ports_test.cpp receipt_test.cpp secret_bytes_test.cpp
                        sealed_content_codec_test.cpp secure_buffer_test.cpp)

target_link_libraries(dgds-test-core PRIVATE dgds::core)
