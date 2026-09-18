#pragma once

#include <dgds/core/models/publication_id.h>
#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/sealed_content.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user_id.h>

namespace dgds::core {

struct PurchaseRecord {
  PurchaseId purchase_id;
  UserId user_id;
  PublicationId publication_id;
  Timestamp purchased_at;
  SealedContent wrapped_blob_key;
};

} // namespace dgds::core
