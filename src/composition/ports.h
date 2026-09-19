#pragma once

#include "configuration.h"

#include <dgds/core/ports/blob_store.h>
#include <dgds/core/ports/identity_registry.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>

#include <memory>

namespace dgds::app {

struct ServerPorts {
  std::unique_ptr<core::BlobStore> blobs;
  std::unique_ptr<core::KeyStore> keys;
  std::unique_ptr<core::MetadataRegistry> metadata;
  std::unique_ptr<core::IdentityRegistry> identities;

  [[nodiscard]] bool complete() const {
    return blobs != nullptr && keys != nullptr && metadata != nullptr && identities != nullptr;
  }
};

[[nodiscard]] ServerPorts make_server_ports(const Configuration &configuration);

} // namespace dgds::app
