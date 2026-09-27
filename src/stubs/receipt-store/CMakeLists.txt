add_library(dgds-stub-receipt-store STATIC "src/file_receipt_store.cpp")
add_library(dgds::stub::receipt-store ALIAS dgds-stub-receipt-store)

target_include_directories(
  dgds-stub-receipt-store
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(dgds-stub-receipt-store
                      PUBLIC dgds::client dgds::stub::support
                      PRIVATE dgds::warnings)
