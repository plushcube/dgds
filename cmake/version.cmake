function(dgds_version_parts version)
  string(REPLACE "." ";" parts "${version}")
  list(LENGTH parts part_count)
  if(NOT part_count EQUAL 3)
    message(FATAL_ERROR "Версия ${version} должна иметь вид MAJOR.MINOR.PATCH")
  endif()

  list(GET parts 0 major)
  list(GET parts 1 minor)
  list(GET parts 2 patch)

  foreach(part major minor patch)
    if(NOT "${${part}}" MATCHES "^[0-9]+$")
      message(FATAL_ERROR "Версия ${version} содержит нечисловую часть")
    endif()
  endforeach()

  set(DGDS_VERSION_MAJOR
      "${major}"
      PARENT_SCOPE)
  set(DGDS_VERSION_MINOR
      "${minor}"
      PARENT_SCOPE)
  set(DGDS_VERSION_PATCH
      "${patch}"
      PARENT_SCOPE)
endfunction()

function(dgds_apply_version target component version)
  dgds_version_parts("${version}")

  set(DGDS_VERSION_NAMESPACE "dgds::${component}")
  set(DGDS_VERSION "${version}")

  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/dgds/${component}/version.h" @ONLY)

  set_target_properties(${target} PROPERTIES VERSION "${version}" SOVERSION
                                             "${DGDS_VERSION_MAJOR}")

  target_include_directories(
    ${target} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
endfunction()

function(dgds_generate_product_version version)
  dgds_version_parts("${version}")

  set(DGDS_VERSION_NAMESPACE "dgds")
  set(DGDS_VERSION "${version}")

  configure_file("${CMAKE_SOURCE_DIR}/cmake/version.h.in"
                 "${PROJECT_BINARY_DIR}/include/dgds/version.h" @ONLY)
endfunction()
