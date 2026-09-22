#include <dgds/stubs/key_store/file_key_store.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/stubs/support/file_storage.h>

#include <expected>
#include <utility>

namespace dgds::stubs {

core::Result<core::SymmetricKey> FileKeyStore::load_master_key() const {
  std::error_code status;
  const bool present = std::filesystem::exists(m_master_key, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (!present) {
    return std::unexpected(core::CoreError::key_not_found);
  }

  return load_secret(m_master_key);
}

core::Result<core::SymmetricKey> FileKeyStore::create_master_key() const {
  std::error_code status;
  const bool present = std::filesystem::exists(m_master_key, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (!present) {
    const auto existing = list_files(m_root);

    if (!existing.has_value()) {
      return std::unexpected(existing.error());
    }

    if (!existing->empty()) {
      return std::unexpected(core::CoreError::key_not_found);
    }
  }

  return load_or_create_secret(m_master_key);
}

core::Result<core::SymmetricKey> FileKeyStore::load_file_key(const core::ContentIdentity &identity) const {
  const auto master = load_master_key();

  if (!master.has_value()) {
    return std::unexpected(master.error());
  }

  const auto stored = load_file(path_of(identity), core::CoreError::key_not_found);

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  const auto sealed = core::decode_sealed_content(stored.value());

  if (!sealed.has_value()) {
    return std::unexpected(core::CoreError::key_malformed);
  }

  return core::unwrap_key(sealed.value(), master.value(), core::as_content(identity));
}

core::Result<core::SymmetricKey> FileKeyStore::obtain_file_key(const core::ContentIdentity &identity) const {
  auto existing = load_file_key(identity);

  if (existing.has_value()) {
    return existing;
  }

  if (existing.error() != core::CoreError::key_not_found) {
    return std::unexpected(existing.error());
  }

  const auto master = create_master_key();

  if (!master.has_value()) {
    return std::unexpected(master.error());
  }

  auto generated = core::generate_key();

  if (!generated.has_value()) {
    return std::unexpected(generated.error());
  }

  const auto wrapped = core::wrap_key(generated.value(), master.value(), core::as_content(identity));

  if (!wrapped.has_value()) {
    return std::unexpected(wrapped.error());
  }

  const auto encoded = core::encode_sealed_content(wrapped.value());

  if (!encoded.has_value()) {
    return std::unexpected(core::CoreError::key_malformed);
  }

  const auto stored = store_file(path_of(identity), encoded.value());

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  return std::move(generated.value());
}

core::Result<core::SealedContent> FileKeyStore::seal(const core::ContentIdentity &identity,
                                                     const core::SecureBuffer &plaintext,
                                                     core::Content associated_data) {
  const auto file_key = obtain_file_key(identity);

  if (!file_key.has_value()) {
    return std::unexpected(file_key.error());
  }

  return core::encrypt(plaintext.view(), file_key.value(), associated_data);
}

core::Result<core::SecureBuffer> FileKeyStore::open(const core::ContentIdentity &identity,
                                                    const core::SealedContent &sealed, core::Content associated_data) {
  const auto file_key = load_file_key(identity);

  if (!file_key.has_value()) {
    return std::unexpected(file_key.error());
  }

  return core::decrypt(sealed, file_key.value(), associated_data);
}

core::Result<core::SealedContent> FileKeyStore::wrap(const core::ContentIdentity &identity,
                                                     const core::SymmetricKey &purchase_key,
                                                     core::Content associated_data) {
  const auto file_key = load_file_key(identity);

  if (!file_key.has_value()) {
    return std::unexpected(file_key.error());
  }

  return core::wrap_key(file_key.value(), purchase_key, associated_data);
}

} // namespace dgds::stubs
