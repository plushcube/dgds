#pragma once

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/ports/key_store.h>

#include <filesystem>
#include <string>
#include <utility>

namespace dgds::stubs {

class FileKeyStore : public core::KeyStore {
public:
  FileKeyStore(std::filesystem::path master_key, std::filesystem::path root)
      : m_master_key(std::move(master_key)), m_root(std::move(root)) {}

  [[nodiscard]] core::Result<core::SealedContent> seal(const core::ContentIdentity &identity,
                                                       const core::SecureBuffer &plaintext,
                                                       core::Content associated_data) override;
  [[nodiscard]] core::Result<core::SecureBuffer> open(const core::ContentIdentity &identity,
                                                      const core::SealedContent &sealed,
                                                      core::Content associated_data) override;
  [[nodiscard]] core::Result<core::SealedContent> wrap(const core::ContentIdentity &identity,
                                                       const core::SymmetricKey &purchase_key,
                                                       core::Content associated_data) override;

private:
  [[nodiscard]] std::filesystem::path path_of(const core::ContentIdentity &identity) const {
    return m_root / (core::to_hex(identity) + k_key_suffix);
  }

  [[nodiscard]] core::Result<core::SymmetricKey> load_master_key() const;
  [[nodiscard]] core::Result<core::SymmetricKey> create_master_key() const;
  [[nodiscard]] core::Result<core::SymmetricKey> load_file_key(const core::ContentIdentity &identity) const;
  [[nodiscard]] core::Result<core::SymmetricKey> obtain_file_key(const core::ContentIdentity &identity) const;

  static constexpr const char *k_key_suffix = ".key";

  std::filesystem::path m_master_key;
  std::filesystem::path m_root;
};

} // namespace dgds::stubs
