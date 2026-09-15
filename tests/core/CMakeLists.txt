dgds_add_test_area(core aead_test.cpp canonical_form_test.cpp
                        content_identity_test.cpp mark_codec_test.cpp)

target_link_libraries(dgds-test-core PRIVATE dgds::core)
