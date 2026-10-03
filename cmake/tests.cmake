include(GoogleTest)

# Предел времени — это детектор зависания, а не бюджет: он должен быть заметно выше самого долгого
# честного теста. Дольше всех идёт HttpClientFixture.DeliversContentOfLimitSize с четырьмя
# мегабайтами содержимого: на TSan около пятнадцати секунд, на ASan около трёх. Отсюда тридцать.
set(DGDS_TEST_TIMEOUT_SECONDS
    30
    CACHE STRING "Предельное время одного теста, секунды"
)
set(DGDS_TEST_DISCOVERY_TIMEOUT_SECONDS
    60
    CACHE STRING "Предельное время обнаружения тестов, секунды"
)

function(dgds_add_test_area area)
  if(NOT ARGN)
    message(FATAL_ERROR "Для области ${area} не указан ни один файл тестов")
  endif()

  set(target "dgds-test-${area}")

  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE GTest::gtest_main dgds::warnings dgds::test-support)

  # Предел времени выставляется каждому тесту: тогда зависание падает с ошибкой в любом запуске,
  # включая локальный, а не висит до лимита джобы.
  gtest_discover_tests(
    ${target}
    PROPERTIES
    TIMEOUT ${DGDS_TEST_TIMEOUT_SECONDS} DISCOVERY_TIMEOUT ${DGDS_TEST_DISCOVERY_TIMEOUT_SECONDS}
  )
endfunction()
