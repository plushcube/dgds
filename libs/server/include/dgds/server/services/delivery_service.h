#pragma once

#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/blob_store.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class DeliveryService {
public:
  DeliveryService(core::BlobStore &blobs, core::MetadataRegistry &metadata) : m_blobs(blobs), m_metadata(metadata) {}

  [[nodiscard]] core::Result<core::Package> fetch_package(const core::UserId &user_id,
                                                          const core::PurchaseId &purchase_id,
                                                          const core::DevicePublicKey &device_key);

private:
  core::BlobStore &m_blobs;
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
