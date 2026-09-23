#pragma once

#include <dgds/core/models/crypto.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstdint>
#include <vector>

namespace dgds::core {

using PurchaseId = std::uint64_t;

struct PurchaseRecord {
  PurchaseId purchase_id;
  UserId user_id;
  PublicationId publication_id;
  Timestamp purchased_at;
};

struct PurchaseSummary {
  PurchaseId purchase_id;
  PublicationSummary publication;
  Timestamp purchased_at;
};

using PurchaseSummaries = std::vector<PurchaseSummary>;

} // namespace dgds::core
