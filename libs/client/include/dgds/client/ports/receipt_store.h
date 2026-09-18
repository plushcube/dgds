#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/result.h>

namespace dgds::client {

using core::Content;
using core::ContentBuffer;
using core::PurchaseId;
using core::Result;

class ReceiptStore {
public:
  ReceiptStore() = default;
  ReceiptStore(const ReceiptStore &) = delete;
  ReceiptStore &operator=(const ReceiptStore &) = delete;
  ReceiptStore(ReceiptStore &&) = delete;
  ReceiptStore &operator=(ReceiptStore &&) = delete;
  virtual ~ReceiptStore() = default;

  [[nodiscard]] virtual Result<void> save(const PurchaseId &purchase_id, Content blob) = 0;
  [[nodiscard]] virtual Result<ContentBuffer> load(const PurchaseId &purchase_id) = 0;
};

} // namespace dgds::client
