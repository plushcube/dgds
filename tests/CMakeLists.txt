add_subdirectory(support)
add_subdirectory(core)
add_subdirectory(server)
add_subdirectory(stubs)

if(APPLE)
  add_subdirectory(client)
endif()
