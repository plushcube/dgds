include(GoogleTest)

function(dgds_add_test_area area)
  if(NOT ARGN)
    message(FATAL_ERROR "Для области ${area} не указан ни один файл тестов")
  endif()

  set(target "dgds-test-${area}")

  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE GTest::gtest_main dgds::warnings
                                             dgds::test-support)

  gtest_discover_tests(${target})
endfunction()
