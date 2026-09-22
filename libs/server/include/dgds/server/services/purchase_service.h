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

private:
  core::KeyStore &m_keys;
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
