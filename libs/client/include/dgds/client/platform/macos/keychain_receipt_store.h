#pragma once

#include <dgds/client/ports/receipt_store.h>

#include <string>
#include <utility>

namespace dgds::client {

class KeychainReceiptStore : public ReceiptStore {
public:
  explicit KeychainReceiptStore(std::string service) : m_service(std::move(service)) {}

  [[nodiscard]] Result<void> save(const PurchaseId &purchase_id, Content blob) override;
  [[nodiscard]] Result<ContentBuffer> load(const PurchaseId &purchase_id) override;

private:
  std::string m_service;
};

} // namespace dgds::client
