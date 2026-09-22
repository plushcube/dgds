#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/identity.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_receipt_version = 1;
inline constexpr std::uint8_t k_package_version = 1;

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
  DeviceEnvelope wrapped_key;
};

struct ReceiptRecord {
  DevicePublicKey device_key;
  Receipt receipt;
};

struct Package {
  std::uint8_t version;
  SealedContent content;
  SealedContent wrapped_blob_key;
  ContentIdentity identity;
  AuthorPublicKey author_key;
  ContentBuffer author_name;
  SignatureAlgorithm signature_algorithm;
  Signature signature;
};

} // namespace dgds::core
