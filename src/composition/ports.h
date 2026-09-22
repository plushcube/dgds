#pragma once

#include "configuration.h"

#include <dgds/core/ports/blob_store.h>
#include <dgds/core/ports/identity_registry.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>

#include <memory>

namespace dgds::app {

struct ServerPorts {
  std::unique_ptr<core::BlobStore> p_blobs;
  std::unique_ptr<core::KeyStore> p_keys;
  std::unique_ptr<core::MetadataRegistry> p_metadata;
  std::unique_ptr<core::IdentityRegistry> p_identities;

  [[nodiscard]] bool complete() const {
    return p_blobs != nullptr && p_keys != nullptr && p_metadata != nullptr && p_identities != nullptr;
  }
};

[[nodiscard]] ServerPorts make_server_ports(const Configuration &configuration);

} // namespace dgds::app
