#include <dgds/server/services/purchase_service.h>

#include <dgds/core/envelope/keys.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/identifier.h>

#include "summaries.h"

#include <expected>

namespace dgds::server {

core::Result<core::Receipt> PurchaseService::buy(const core::UserId &user_id, const core::PublicationId &publication_id,
                                                 const core::DevicePublicKey &device_key,
                                                 core::Timestamp purchased_at) {
  const auto existing = m_metadata.find_purchase_of(user_id, publication_id);

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

  const auto purchase_id = core::generate_identifier();

  if (!purchase_id.has_value()) {
    return std::unexpected(purchase_id.error());
  }

  const core::PurchaseRecord purchase{.purchase_id = purchase_id.value(),
                                      .user_id = user_id,
                                      .publication_id = publication_id,
                                      .purchased_at = purchased_at};

  const auto added = m_metadata.add_purchase(purchase);

  if (!added.has_value()) {
    return std::unexpected(added.error());
  }

  return issue_receipt(purchase, publication.value(), device_key, purchased_at);
}

core::Result<core::Receipt> PurchaseService::restore_receipt(const core::UserId &user_id,
                                                             const core::PurchaseId &purchase_id,
                                                             const core::DevicePublicKey &device_key,
                                                             core::Timestamp issued_at) {
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

  return issue_receipt(purchase.value(), publication.value(), device_key, issued_at);
}

core::Result<core::Receipt> PurchaseService::issue_receipt(const core::PurchaseRecord &purchase,
                                                           const core::PublicationRecord &publication,
                                                           const core::DevicePublicKey &device_key,
                                                           core::Timestamp issued_at) {
  auto purchase_key = core::generate_key();

  if (!purchase_key.has_value()) {
    return std::unexpected(purchase_key.error());
  }

  const core::Content bound_identity = core::as_content(publication.identity);
  const auto wrapped = m_keys.wrap(publication.identity, purchase_key.value(), bound_identity);

  if (!wrapped.has_value()) {
    return std::unexpected(wrapped.error());
  }

  const core::ReceiptHeader header{.version = core::k_receipt_version,
                                   .purchase_id = purchase.purchase_id,
                                   .user_id = purchase.user_id,
                                   .purchased_at = purchase.purchased_at,
                                   .issued_at = issued_at};

  const auto envelope = core::wrap_receipt_key(purchase_key.value(), device_key, header);

  if (!envelope.has_value()) {
    return std::unexpected(envelope.error());
  }

  const core::Receipt receipt{.header = header, .wrapped_key = envelope.value()};
  const auto saved = m_metadata.save_receipt(
      core::ReceiptRecord{.device_key = device_key, .wrapped_blob_key = wrapped.value(), .receipt = receipt});

  if (!saved.has_value()) {
    return std::unexpected(saved.error());
  }

  return receipt;
}

core::Result<core::PurchaseSummaries> PurchaseService::purchases_of(const core::UserId &user_id) {
  const auto purchases = m_metadata.purchases_of_user(user_id);

  if (!purchases.has_value()) {
    return std::unexpected(purchases.error());
  }

  core::PurchaseSummaries summaries;
  summaries.reserve(purchases->size());

  for (const auto &purchase : purchases.value()) {
    const auto publication = m_metadata.find_publication(purchase.publication_id);

    if (!publication.has_value()) {
      return std::unexpected(publication.error());
    }

    summaries.push_back(core::PurchaseSummary{.purchase_id = purchase.purchase_id,
                                              .publication = summarise_publication(publication.value()),
                                              .purchased_at = purchase.purchased_at});
  }

  return summaries;
}

} // namespace dgds::server
