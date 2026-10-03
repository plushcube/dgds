# Сборка пакетов: `.deb` и `.tar.gz` на Linux, `.tar.gz` на macOS. Пакет несёт оба приложения,
# поэтому распакованного архива достаточно, чтобы поднять стенд «сервер + клиент-пример» и пройти
# сценарий вживую.

set(CPACK_PACKAGE_NAME "dgds")
set(CPACK_PACKAGE_VENDOR "DGDS")
set(CPACK_PACKAGE_CONTACT "plushcube")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/plushcube/dgds")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Сервис защищённой дистрибуции цифрового контента")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dgds")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

if(APPLE)
  set(CPACK_GENERATOR "TGZ")
  set(CPACK_PACKAGE_FILE_NAME "dgds-${PROJECT_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")
else()
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_PACKAGE_FILE_NAME "dgds-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "plushcube")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  # libssl3t64 — имя библиотеки OpenSSL 3 в Ubuntu 24.04, libssl3 — в более ранних выпусках.
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3 | libssl3t64, ca-certificates")
endif()
