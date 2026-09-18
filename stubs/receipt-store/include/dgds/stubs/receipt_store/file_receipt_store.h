#pragma once

#include <dgds/client/ports/receipt_store.h>

#include <filesystem>
#include <string>
#include <utility>

namespace dgds::stubs {

class FileReceiptStore : public client::ReceiptStore {
public:
  explicit FileReceiptStore(std::filesystem::path root) : m_root(std::move(root)) {}

  [[nodiscard]] client::Result<void> save(const client::PurchaseId &purchase_id, client::Content blob) override;
  [[nodiscard]] client::Result<client::ContentBuffer> load(const client::PurchaseId &purchase_id) override;

private:
  [[nodiscard]] std::filesystem::path path_of(const client::PurchaseId &purchase_id) const {
    return m_root / (std::to_string(purchase_id) + k_receipt_suffix);
  }

  static constexpr const char *k_receipt_suffix = ".receipt";

  std::filesystem::path m_root;
};

} // namespace dgds::stubs
