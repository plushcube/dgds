function(dgds_apply_version target component version)
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

  set(DGDS_VERSION "${version}")
  set(DGDS_VERSION_COMPONENT "${component}")

  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/dgds/${component}/version.h" @ONLY)

  set_target_properties(${target} PROPERTIES VERSION "${version}" SOVERSION
                                             "${major}")

  target_include_directories(
    ${target} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
endfunction()
