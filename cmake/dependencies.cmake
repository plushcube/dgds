include(FetchContent)

find_package(OpenSSL 3 REQUIRED)

set(HTTPLIB_INSTALL OFF)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)

# Разрешение имени оставлено синхронным. Асинхронная ветка httplib на glibc держит запрос в стековом
# gaicb, а после таймаута зовёт gai_cancel() и ждёт рабочий поток резолвера без предела (httplib.h,
# ссылка на yhirose/cpp-httplib#2431) — под ThreadSanitizer этот поток падает внутри аллокатора
# TSan, и тест после этого висит до предела ctest. Цена отказа: таймаут соединения больше не
# покрывает DNS, разрешение ограничено таймаутом самого резолвера.
set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF)

FetchContent_Declare(
  httplib URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.56.0.zip
              DOWNLOAD_EXTRACT_TIMESTAMP TRUE EXCLUDE_FROM_ALL
)

FetchContent_Declare(
  nlohmann_json URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.zip
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(httplib nlohmann_json)

if(WITH_TESTS)
  FetchContent_Declare(
    googletest URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
                   DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  if(MSVC)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE
    )
  endif()

  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)
endif()
