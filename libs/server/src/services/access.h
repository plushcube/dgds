#pragma once

#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

struct Access {
  core::PublicationRecord publication;
  core::PurchaseId context_id;
  core::Timestamp granted_at;
};

[[nodiscard]] core::Result<Access> resolve_access(const core::UserId &user_id, const core::PurchaseId &context_id,
                                                  core::MetadataRegistry &metadata);

} // namespace dgds::server
