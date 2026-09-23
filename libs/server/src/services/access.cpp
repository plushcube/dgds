#include "access.h"

#include <expected>

namespace dgds::server {

core::Result<Access> resolve_context(const core::PurchaseId &context_id, core::MetadataRegistry &metadata) {
  const auto purchase = metadata.find_purchase(context_id);

  if (purchase.has_value()) {
    const auto publication = metadata.find_publication(purchase->publication_id);

    if (!publication.has_value()) {
      return std::unexpected(publication.error());
    }

    return Access{.publication = publication.value(),
                  .kind = AccessKind::purchase,
                  .user_id = purchase->user_id,
                  .context_id = context_id,
                  .granted_at = purchase->purchased_at};
  }

  if (purchase.error() != core::CoreError::purchase_not_found) {
    return std::unexpected(purchase.error());
  }

  const auto publication = metadata.find_publication(context_id);

  if (!publication.has_value()) {
    return std::unexpected(purchase.error());
  }

  return Access{.publication = publication.value(),
                .kind = AccessKind::author,
                .user_id = publication->author_id,
                .context_id = context_id,
                .granted_at = publication->published_at};
}

core::Result<Access> resolve_access(const core::UserId &user_id, const core::PurchaseId &context_id,
                                    core::MetadataRegistry &metadata) {
  const auto access = resolve_context(context_id, metadata);

  if (!access.has_value()) {
    return std::unexpected(access.error());
  }

  if (access->user_id != user_id) {
    return std::unexpected(core::CoreError::not_permitted);
  }

  return access;
}

} // namespace dgds::server
