#pragma once

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/ports/blob_store.h>

#include <filesystem>
#include <string>
#include <utility>

namespace dgds::stubs {

class FileBlobStore : public core::BlobStore {
public:
  explicit FileBlobStore(std::filesystem::path root) : m_root(std::move(root)) {}

  [[nodiscard]] core::Result<void> store(const core::ContentIdentity &identity,
                                         const core::SealedContent &blob) override;
  [[nodiscard]] core::Result<core::SealedContent> load(const core::ContentIdentity &identity) override;

private:
  [[nodiscard]] std::filesystem::path path_of(const core::ContentIdentity &identity) const {
    return m_root / (core::to_hex(identity) + k_blob_suffix);
  }

  static constexpr const char *k_blob_suffix = ".blob";

  std::filesystem::path m_root;
};

} // namespace dgds::stubs
