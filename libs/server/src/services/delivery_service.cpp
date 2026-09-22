#include <dgds/server/services/delivery_service.h>

#include <expected>

namespace dgds::server {

core::Result<core::Package> DeliveryService::fetch_package(const core::UserId &user_id,
                                                           const core::PurchaseId &purchase_id) {
  const auto purchase = m_metadata.find_purchase(purchase_id);

  if (!purchase.has_value()) {
    return std::unexpected(purchase.error());
  }

  if (purchase->user_id != user_id) {
    return std::unexpected(core::CoreError::not_permitted);
  }

  const auto publication = m_metadata.find_publication(purchase->publication_id);

  if (!publication.has_value()) {
    return std::unexpected(publication.error());
  }

  const auto content = m_blobs.load(publication->identity);

  if (!content.has_value()) {
    return std::unexpected(content.error());
  }

  return core::Package{.version = core::k_package_version,
                       .content = content.value(),
                       .wrapped_blob_key = purchase->wrapped_blob_key,
                       .identity = publication->identity,
                       .author_key = publication->author_key,
                       .author_name = publication->author_name,
                       .signature_algorithm = publication->signature_algorithm,
                       .signature = publication->signature};
}

} // namespace dgds::server
