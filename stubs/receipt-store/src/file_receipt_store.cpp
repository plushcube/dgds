#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <dgds/stubs/support/file_storage.h>

#include <expected>

namespace dgds::stubs {

client::Result<void> FileReceiptStore::save(const client::PurchaseId &purchase_id, client::Content blob) {
  return store_file(path_of(purchase_id), blob);
}

client::Result<client::ContentBuffer> FileReceiptStore::load(const client::PurchaseId &purchase_id) {
  return load_file(path_of(purchase_id), core::CoreError::receipt_not_found);
}

} // namespace dgds::stubs
