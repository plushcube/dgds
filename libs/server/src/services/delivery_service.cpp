#include <dgds/server/services/delivery_service.h>

#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/mark.h>
#include <dgds/core/watermark/mark_channel.h>

#include "access.h"

#include <cstring>
#include <expected>
#include <optional>

namespace dgds::server {

core::Result<core::SealedContent> DeliveryService::mark_for_purchase(const core::PublicationRecord &publication,
                                                                     const core::SealedContent &stored,
                                                                     const core::PurchaseId &context_id) {
  const core::Content bound_identity = core::as_content(publication.identity);
  const auto plaintext = m_keys.open(publication.identity, stored, bound_identity);

  if (!plaintext.has_value()) {
    return std::unexpected(plaintext.error());
  }

  const std::optional<core::ContentBuffer> marked =
      core::embed_mark(plaintext->view(), core::Mark{.purchase_id = context_id, .version = core::k_mark_version});

  if (!marked.has_value()) {
    return stored;
  }

  core::SecureBuffer buffer(marked->size());

  if (!marked->empty()) {
    std::memcpy(buffer.data(), marked->data(), marked->size());
  }

  return m_keys.seal(publication.identity, buffer, bound_identity);
}

core::Result<core::Package> DeliveryService::fetch_package(const core::UserId &user_id,
                                                           const core::PurchaseId &context_id,
                                                           const core::DevicePublicKey &device_key) {
  const auto access = resolve_access(user_id, context_id, m_metadata);

  if (!access.has_value()) {
    return std::unexpected(access.error());
  }

  const auto receipt = m_metadata.find_receipt(context_id, device_key);

  if (!receipt.has_value()) {
    return std::unexpected(receipt.error());
  }

  const auto stored = m_blobs.load(access->publication.identity);

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  const auto content = mark_for_purchase(access->publication, stored.value(), context_id);

  if (!content.has_value()) {
    return std::unexpected(content.error());
  }

  return core::Package{.version = core::k_package_version,
                       .content = content.value(),
                       .wrapped_blob_key = receipt->wrapped_blob_key,
                       .identity = access->publication.identity,
                       .author_key = access->publication.author_key,
                       .author_name = access->publication.author_name,
                       .signature_algorithm = access->publication.signature_algorithm,
                       .signature = access->publication.signature};
}

} // namespace dgds::server
