#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

namespace dgds::server {

enum class AccessKind {
  purchase,
  author,
};

struct Attribution {
  AccessKind kind;
  core::PurchaseId context_id;
  core::UserId user_id;
  core::ContentBuffer user_name;
  core::PublicationId publication_id;
  core::ContentBuffer title;
  core::Timestamp granted_at;
};

} // namespace dgds::server
