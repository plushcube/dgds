#pragma once

#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/sealed_content.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user_id.h>

#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_receipt_version = 1;

struct ReceiptHeader {
  std::uint8_t version;
  PurchaseId purchase_id;
  UserId user_id;
  Timestamp purchased_at;
  Timestamp issued_at;

  bool operator==(const ReceiptHeader &) const = default;
};

struct Receipt {
  ReceiptHeader header;
  SealedContent wrapped_key;
};

} // namespace dgds::core
