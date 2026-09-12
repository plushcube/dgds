add_library(dgds-stub-receipt-store INTERFACE)
add_library(dgds::stub::receipt-store ALIAS dgds-stub-receipt-store)

target_link_libraries(dgds-stub-receipt-store INTERFACE dgds::client)
