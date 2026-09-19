#include "ports.h"

#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <memory>

namespace dgds::app {
namespace {

constexpr const char *k_blobs_directory = "blobs";
constexpr const char *k_keys_directory = "keys";
constexpr const char *k_metadata_directory = "metadata";
constexpr const char *k_identities_directory = "identities";

} // namespace

ServerPorts make_server_ports(const Configuration &configuration) {
  return ServerPorts{
      .blobs = std::make_unique<stubs::FileBlobStore>(configuration.storage_root / k_blobs_directory),
      .keys = std::make_unique<stubs::FileKeyStore>(configuration.master_key,
                                                    configuration.storage_root / k_keys_directory),
      .metadata = std::make_unique<stubs::FileMetadataRegistry>(configuration.storage_root / k_metadata_directory),
      .identities = std::make_unique<stubs::FileIdentityRegistry>(configuration.storage_root / k_identities_directory),
  };
}

} // namespace dgds::app
