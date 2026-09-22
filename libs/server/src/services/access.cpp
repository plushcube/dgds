#include "access.h"

#include <expected>

namespace dgds::server {

core::Result<Access> resolve_access(const core::UserId &user_id, const core::PurchaseId &context_id,
                                    core::MetadataRegistry &metadata) {
  const auto purchase = metadata.find_purchase(context_id);

  if (purchase.has_value()) {
    if (purchase->user_id != user_id) {
      return std::unexpected(core::CoreError::not_permitted);
    }

    const auto publication = metadata.find_publication(purchase->publication_id);

    if (!publication.has_value()) {
      return std::unexpected(publication.error());
    }

    return Access{.publication = publication.value(), .context_id = context_id, .granted_at = purchase->purchased_at};
  }

  if (purchase.error() != core::CoreError::purchase_not_found) {
    return std::unexpected(purchase.error());
  }

  const auto publication = metadata.find_publication(context_id);

  if (!publication.has_value()) {
    return std::unexpected(purchase.error());
  }

  if (publication->author_id != user_id) {
    return std::unexpected(core::CoreError::not_permitted);
  }

  return Access{.publication = publication.value(), .context_id = context_id, .granted_at = publication->published_at};
}

} // namespace dgds::server
