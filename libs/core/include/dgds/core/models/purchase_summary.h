#pragma once

#include <dgds/core/models/publication_summary.h>
#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/timestamp.h>

namespace dgds::core {

struct PurchaseSummary {
  PurchaseId purchase_id;
  PublicationSummary publication;
  Timestamp purchased_at;
};

} // namespace dgds::core
