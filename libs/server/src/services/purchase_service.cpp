#include <dgds/server/services/purchase_service.h>

#include <dgds/core/envelope/keys.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/identifier.h>

#include <expected>

namespace dgds::server {

core::Result<core::Receipt> PurchaseService::buy(const core::UserId &user_id, const core::PublicationId &publication_id,
                                                 const core::DevicePublicKey &device_key,
                                                 core::Timestamp purchased_at) {
  auto existing = m_metadata.find_purchase_of(user_id, publication_id);

  if (existing.has_value()) {
    const auto stored = m_metadata.find_receipt(existing->purchase_id, device_key);

    if (!stored.has_value()) {
      return std::unexpected(stored.error());
    }

    return stored->receipt;
  }

  if (existing.error() != core::CoreError::purchase_not_found) {
    return std::unexpected(existing.error());
  }

  const auto publication = m_metadata.find_publication(publication_id);

  if (!publication.has_value()) {
    return std::unexpected(publication.error());
  }

  auto purchase_key = core::generate_key();

  if (!purchase_key.has_value()) {
    return std::unexpected(purchase_key.error());
  }

  const core::Content bound_identity = core::as_content(publication->identity);
  const auto wrapped = m_keys.wrap(publication->identity, purchase_key.value(), bound_identity);

  if (!wrapped.has_value()) {
    return std::unexpected(wrapped.error());
  }

  const auto purchase_id = core::generate_identifier();

  if (!purchase_id.has_value()) {
    return std::unexpected(purchase_id.error());
  }

  const core::PurchaseRecord purchase{.purchase_id = purchase_id.value(),
                                      .user_id = user_id,
                                      .publication_id = publication_id,
                                      .purchased_at = purchased_at,
                                      .wrapped_blob_key = wrapped.value()};

  const auto added = m_metadata.add_purchase(purchase);

  if (!added.has_value()) {
    return std::unexpected(added.error());
  }

  const core::ReceiptHeader header{.version = core::k_receipt_version,
                                   .purchase_id = purchase_id.value(),
                                   .user_id = user_id,
                                   .purchased_at = purchased_at,
                                   .issued_at = purchased_at};

  const auto envelope = core::wrap_receipt_key(purchase_key.value(), device_key, header);

  if (!envelope.has_value()) {
    return std::unexpected(envelope.error());
  }

  const core::Receipt receipt{.header = header, .wrapped_key = envelope.value()};
  const auto recorded = m_metadata.add_receipt(core::ReceiptRecord{.device_key = device_key, .receipt = receipt});

  if (!recorded.has_value()) {
    return std::unexpected(recorded.error());
  }

  return receipt;
}

} // namespace dgds::server
