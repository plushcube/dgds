#pragma once

#include <dgds/core/models/crypto.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstdint>

namespace dgds::core {

using PurchaseId = std::uint64_t;

struct PurchaseRecord {
  PurchaseId purchase_id;
  UserId user_id;
  PublicationId publication_id;
  Timestamp purchased_at;
  SealedContent wrapped_blob_key;
};

struct PurchaseSummary {
  PurchaseId purchase_id;
  PublicationSummary publication;
  Timestamp purchased_at;
};

} // namespace dgds::core
