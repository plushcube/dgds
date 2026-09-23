#pragma once

#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class PurchaseService {
public:
  PurchaseService(core::KeyStore &keys, core::MetadataRegistry &metadata) : m_keys(keys), m_metadata(metadata) {}

  [[nodiscard]] core::Result<core::Receipt> buy(const core::UserId &user_id, const core::PublicationId &publication_id,
                                                const core::DevicePublicKey &device_key, core::Timestamp purchased_at);

  [[nodiscard]] core::Result<core::PurchaseSummaries> purchases_of(const core::UserId &user_id);

  [[nodiscard]] core::Result<core::Receipt> restore_receipt(const core::UserId &user_id,
                                                            const core::PurchaseId &context_id,
                                                            const core::DevicePublicKey &device_key,
                                                            core::Timestamp issued_at);

private:
  [[nodiscard]] core::Result<core::Receipt>
  issue_receipt(const core::PurchaseId &context_id, const core::UserId &user_id,
                const core::PublicationRecord &publication, core::Timestamp granted_at,
                const core::DevicePublicKey &device_key, core::Timestamp issued_at);

  core::KeyStore &m_keys;
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
